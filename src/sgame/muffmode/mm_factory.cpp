// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_debug.h"
#include "muffmode/mm_factory.h"
#include "muffmode/mm_factory_builtin.h"
#include "muffmode/mm_gt_session.h"
#include "muffmode/mm_map_pool.h"
#include "muffmode/mm_util.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace muffmode {
namespace factory {

namespace {

std::vector<mm_factory_t> s_registry;
bool s_loaded = false;

const mm_factory_t *FindIn(const std::vector<mm_factory_t> &registry,
	std::string_view id)
{
	for (const mm_factory_t &entry : registry)
		if (MM_GTEqualsI(entry.id, id))
			return &entry;
	return nullptr;
}

mm_factory_t *FindMutableIn(std::vector<mm_factory_t> &registry,
	std::string_view id)
{
	for (mm_factory_t &entry : registry)
		if (MM_GTEqualsI(entry.id, id))
			return &entry;
	return nullptr;
}

// Later definitions replace earlier ones by id, so an operator's file can
// re-point a built-in without losing its place in listings.
void Install(std::vector<mm_factory_t> &registry, mm_factory_t &&entry)
{
	if (mm_factory_t *existing = FindMutableIn(registry, entry.id)) {
		if (!existing->builtin)
			gi.Com_PrintFmt("factories: {} redefines {}.\n",
				entry.source.c_str(), entry.id.c_str());
		*existing = std::move(entry);
		return;
	}
	registry.push_back(std::move(entry));
}

void AddBuiltins(std::vector<mm_factory_t> &registry)
{
	for (const mm_builtin_factory_t &builtin : MM_BUILTIN_FACTORIES) {
		mm_factory_t entry;
		entry.id = builtin.id;
		entry.title = builtin.title;
		entry.desc = builtin.desc;
		entry.source = "built-in";
		entry.base = builtin.base;
		entry.builtin = true;
		entry.hidden = MM_IsHiddenFactoryId(entry.id);
		registry.push_back(std::move(entry));
	}
}

struct read_result_t {
	bool		ok = false;
	std::string	text;
	std::string	error;
};

read_result_t ReadFactoryFile(const std::string &leaf)
{
	read_result_t result;

	if (!muffmode::map_pool::IsSafeConfigLeaf(leaf)) {
		result.error = "unsafe file name";
		return result;
	}

	cvar_t *base_dir_cvar = gi.cvar("basedir", ".", CVAR_NOFLAGS);
	const char *base_dir = base_dir_cvar && base_dir_cvar->string &&
		*base_dir_cvar->string ? base_dir_cvar->string : ".";
	const std::string path =
		(std::filesystem::path(base_dir) / "baseq2" / leaf).string();

	auto file = muffmode::OpenFile(path.c_str(), "rb");
	if (!file) {
		result.error = "could not open " + path;
		return result;
	}

	if (std::fseek(file.get(), 0, SEEK_END) != 0) {
		result.error = "could not seek " + path;
		return result;
	}
	const long length = std::ftell(file.get());
	if (length < 0 ||
		static_cast<size_t>(length) > MM_FACTORY_MAX_BYTES ||
		std::fseek(file.get(), 0, SEEK_SET) != 0) {
		result.error = "invalid size for " + path;
		return result;
	}

	result.text.assign(static_cast<size_t>(length), '\0');
	if (length > 0) {
		const size_t read =
			std::fread(result.text.data(), 1, result.text.size(), file.get());
		if (read != result.text.size() || std::ferror(file.get()) != 0) {
			result.text.clear();
			result.error = "could not read " + path;
			return result;
		}
	}

	result.ok = true;
	return result;
}

// Flattens `inherit` against what is already registered. A parent is always an
// earlier definition and is therefore already flattened, so this is a single
// merge rather than a recursion: there is no cycle to detect and no recursion
// depth to bound. Inheritance binds at load time, so a later file redefining a
// parent does not retroactively change its children.
bool ResolveInheritance(std::vector<mm_factory_t> &registry,
	mm_factory_t &entry, const std::string &parent_id, std::string &error)
{
	const mm_factory_t *parent = FindIn(registry, parent_id);
	if (!parent) {
		error = "inherits unknown factory `" + parent_id + "`";
		return false;
	}
	if (parent->base != entry.base) {
		error = "inherits `" + parent_id + "`, which is a " +
			gt_short_name[(int)parent->base] + " factory";
		return false;
	}
	if (parent->overrides.size() + entry.overrides.size() >
		MM_FACTORY_MAX_OVERRIDES) {
		error = "exceeds " + std::to_string(MM_FACTORY_MAX_OVERRIDES) +
			" overrides after inheriting `" + parent_id + "`";
		return false;
	}

	entry.overrides = MM_MergeOverrides(parent->overrides, entry.overrides);
	entry.scope = MM_MergeScope(parent->scope, entry.scope);
	if (entry.ruleset == 0)
		entry.ruleset = parent->ruleset;
	if (entry.desc.empty())
		entry.desc = parent->desc;
	return true;
}

// Returns false when the document itself was unusable, so the caller can keep
// the previously working registry rather than publishing a partial one. An
// individual rejected block is not a file failure -- it is named and skipped.
bool LoadFile(std::vector<mm_factory_t> &registry, const std::string &leaf)
{
	const read_result_t read = ReadFactoryFile(leaf);
	if (!read.ok) {
		gi.Com_PrintFmt("factories: {}.\n", read.error.c_str());
		return false;
	}

	const factory_parse_result_t parsed =
		MM_ParseFactoryDocument(read.text, leaf);

	for (const std::string &error : parsed.errors)
		gi.Com_PrintFmt("factories: {}\n", error.c_str());

	if (!parsed.valid) {
		gi.Com_PrintFmt(
			"factories: {} was not loaded; fix the errors above.\n",
			leaf.c_str());
		return false;
	}

	size_t accepted = 0;
	for (const parsed_factory_t &source : parsed.factories) {
		const mm_gt_lookup_t base = MM_GTFindByShortName(source.base_token);
		if (!base.found || base.gt == GT_NONE) {
			gi.Com_PrintFmt(
				"factories: {}:{}: `{}` names an unknown base gametype `{}`.\n",
				leaf.c_str(), source.line, source.id.c_str(),
				source.base_token.c_str());
			continue;
		}
		if (MM_GetGametypeAvailability(base.gt) != gametype_avail_t::Enabled) {
			gi.Com_PrintFmt(
				"factories: {}:{}: `{}` names the unavailable gametype `{}`.\n",
				leaf.c_str(), source.line, source.id.c_str(),
				source.base_token.c_str());
			continue;
		}

		mm_factory_t entry;
		entry.id = source.id;
		entry.title = source.title;
		entry.desc = source.desc;
		entry.source = leaf;
		entry.base = base.gt;
		entry.overrides = source.overrides;
		entry.scope = source.scope;
		entry.hidden = source.hidden;

		if (!source.ruleset_token.empty()) {
			const ruleset_t ruleset =
				RS_IndexFromString(source.ruleset_token.c_str());
			if (ruleset == RS_NONE) {
				gi.Com_PrintFmt(
					"factories: {}:{}: `{}` names an unknown ruleset `{}`.\n",
					leaf.c_str(), source.line, source.id.c_str(),
					source.ruleset_token.c_str());
				continue;
			}
			entry.ruleset = (int)ruleset;
		}

		if (!source.inherit.empty()) {
			std::string error;
			if (!ResolveInheritance(registry, entry, source.inherit, error)) {
				gi.Com_PrintFmt("factories: {}:{}: `{}` {}.\n",
					leaf.c_str(), source.line, source.id.c_str(),
					error.c_str());
				continue;
			}
		}

		Install(registry, std::move(entry));
		accepted++;
	}

	gi.Com_PrintFmt("factories: loaded {} definition{} from {}{}.\n",
		accepted, accepted == 1 ? "" : "s", leaf.c_str(),
		parsed.rejected_blocks
			? G_Fmt(" ({} rejected)", parsed.rejected_blocks).data() : "");
	return true;
}

// Writes the structured directives out to the cvars the map and lobby systems
// already read. Every write goes through the override ledger, so `factory none`
// or a switch to a factory that names no rotation puts the operator's own
// server-base.cfg values back.
int ApplyScope(const mm_factory_scope_t &scope)
{
	using muffmode::gametype::cvar_owner_t;
	using muffmode::gametype::cvar_write_t;

	const auto join = [](const std::vector<std::string> &maps) {
		std::string out;
		for (const std::string &map : maps) {
			if (!out.empty())
				out += ' ';
			out += map;
		}
		return out;
	};
	const auto write = [](const char *name, const std::string &value) {
		return muffmode::gametype::MM_GT_WriteOwnedCvar(
			name, value.c_str(), cvar_owner_t::Factory) == cvar_write_t::Applied
			? 1 : 0;
	};

	int applied = 0;
	if (!scope.maps.empty())
		applied += write("g_map_list", join(scope.maps));
	if (!scope.mappool.empty())
		applied += write("g_map_pool", join(scope.mappool));
	if (scope.rotation != mm_factory_rotation_t::Inherit)
		applied += write("g_map_list_shuffle",
			std::to_string(static_cast<int>(scope.rotation)));
	if (scope.min_players >= 0)
		applied += write("minplayers", std::to_string(scope.min_players));
	if (scope.max_players >= 0)
		applied += write("maxplayers", std::to_string(scope.max_players));
	return applied;
}

// The cvar's own buffer; stable until something writes g_factory. Returning a
// std::string here would allocate on a path the published-name refresh walks
// every frame.
const char *ActiveId()
{
	return g_factory && g_factory->string ? g_factory->string : "";
}

} // namespace

bool MM_Factory_LoadRegistry()
{
	// Staged: a bad edit leaves the working registry in place rather than
	// dropping the server to no factories at all.
	std::vector<mm_factory_t> staged;
	staged.reserve(MM_BUILTIN_FACTORIES.size() + MM_FACTORY_MAX_FACTORIES);
	AddBuiltins(staged);

	bool all_files_ok = true;
	const char *files = g_factory_file && g_factory_file->string
		? g_factory_file->string : "";
	std::string_view remaining(files);
	while (!remaining.empty()) {
		while (!remaining.empty() && MM_IsAsciiWhitespace(remaining.front()))
			remaining.remove_prefix(1);
		if (remaining.empty())
			break;
		size_t end = 0;
		while (end < remaining.size() && !MM_IsAsciiWhitespace(remaining[end]))
			end++;
		if (!LoadFile(staged, std::string(remaining.substr(0, end))))
			all_files_ok = false;
		remaining.remove_prefix(end);
	}

	// A document-level error contributes nothing, so publishing the staged
	// registry anyway would silently drop every operator-defined factory and
	// leave only the built-ins -- which then clears the operator's selection as
	// "unknown". Keep what was working and say so.
	if (!all_files_ok && s_loaded) {
		gi.Com_PrintFmt(
			"factories: keeping the previously loaded registry ({} definitions).\n",
			s_registry.size());
		return false;
	}

	s_registry = std::move(staged);
	s_loaded = true;
	return all_files_ok;
}

size_t MM_Factory_Count()
{
	return s_registry.size();
}

const mm_factory_t *MM_Factory_Find(std::string_view id)
{
	if (id.empty())
		return nullptr;
	return FindIn(s_registry, id);
}

const mm_factory_t *MM_Factory_Active()
{
	return MM_Factory_Find(ActiveId());
}

int MM_Factory_BaseGametype(std::string_view id)
{
	const mm_factory_t *entry = MM_Factory_Find(id);
	return entry ? (int)entry->base : -1;
}

std::string MM_Factory_List(gametype_t filter)
{
	std::string out;
	for (const mm_factory_t &entry : s_registry) {
		if (entry.hidden)
			continue;
		if (filter != GT_NONE && entry.base != filter)
			continue;
		if (!out.empty())
			out += ' ';
		out += entry.id;
	}
	return out;
}

bool MM_Factory_IsVotable(std::string_view id)
{
	const mm_factory_t *entry = MM_Factory_Find(id);
	if (!entry || entry->hidden)
		return false;

	const char *list = g_votable_factories && g_votable_factories->string
		? g_votable_factories->string : "";
	if (!list[0])
		return true;

	const char *cursor = list;
	const char *token = nullptr;
	while ((token = COM_Parse(&cursor)) && *token)
		if (MM_GTEqualsI(token, entry->id))
			return true;
	return false;
}

void MM_Factory_ApplySelection()
{
	using muffmode::gametype::cvar_owner_t;
	using muffmode::gametype::cvar_write_t;

	if (!s_loaded)
		MM_Factory_LoadRegistry();

	// Factories only describe deathmatch gameplay. Without this gate a
	// g_factory left in a server config would rewrite match limits and mutators
	// for a campaign or co-op session, which has no way to ask for one.
	if (!deathmatch || !deathmatch->integer)
		return;

	// Idempotent: hand back whatever the previously applied factory owns before
	// recording new baselines. Without this, applying twice would record our own
	// value as the "prior" one and the next switch would restore to it.
	muffmode::gametype::MM_GT_RestoreOwned();

	const char *id = ActiveId();
	if (!id[0]) {
		// No factory: make sure a title left over from a previous selection is
		// not still advertised in serverinfo.
		gi.cvar_forceset("g_factory_title", "");
		muffmode::gametype::MM_GT_MarkNameDirty();
		return;
	}

	const mm_factory_t *entry = MM_Factory_Find(id);
	if (!entry) {
		// Never guess. Clear the selection and say what was available.
		gi.Com_PrintFmt("factories: unknown factory \"{}\"; ignoring it.\n", id);
		const std::string available = MM_Factory_List(GT_NONE);
		if (!available.empty())
			gi.Com_PrintFmt("factories: available: {}\n", available.c_str());
		gi.cvar_forceset("g_factory", "");
		gi.cvar_forceset("g_factory_title", "");
		muffmode::gametype::MM_GT_MarkNameDirty();
		return;
	}

	// A factory is written for one base gametype. The transaction enforces that
	// for a request, but a selection can also arrive straight from a server
	// config or an external cvar write, and applying a CTF preset to a Clan
	// Arena server would silently mis-configure it.
	// Compared against the *configured* gametype, not the effective one: a
	// MuffMode Arena selection reads as Deathmatch until the map passes the RA2
	// contract, and an arena factory must not be discarded for that.
	const int configured = muffmode::gametype::g_gt_live.configured;
	if ((int)entry->base != configured) {
		gi.Com_PrintFmt(
			"factories: {} is a {} factory but the gametype is {}; ignoring it.\n",
			entry->id.c_str(), gt_short_name[(int)entry->base],
			gt_short_name[configured]);
		gi.cvar_forceset("g_factory", "");
		gi.cvar_forceset("g_factory_title", "");
		muffmode::gametype::MM_GT_MarkNameDirty();
		return;
	}

	// Queued by the transaction to run before the map command, so these
	// overrides layer on top of the operator's own configs, the ledger's baseline
	// is the value those configs produced, and latched settings are written while
	// there is still a map load ahead of them to take effect at.
	int applied = 0;
	int rejected = 0;
	for (const parsed_override_t &override_entry : entry->overrides) {
		const cvar_write_t result = muffmode::gametype::MM_GT_WriteOwnedCvar(
			override_entry.name.c_str(), override_entry.value.c_str(),
			cvar_owner_t::Factory);
		if (result == cvar_write_t::Applied)
			applied++;
		else if (result != cvar_write_t::Unchanged)
			rejected++;
	}

	if (entry->ruleset > 0)
		muffmode::gametype::MM_GT_WriteOwnedCvar("g_ruleset",
			G_Fmt("{}", entry->ruleset).data(), cvar_owner_t::Factory);

	// The structured directives go through the same ledger as the overrides, so
	// the operator's own rotation and player limits come back on a switch. They
	// are joined here rather than at parse time because the ledger stores what
	// was actually written.
	applied += ApplyScope(entry->scope);

	gi.cvar_forceset("g_factory_title", entry->title.c_str());
	muffmode::gametype::MM_GT_MarkNameDirty();

	gi.Com_PrintFmt("factories: {} ({}) from {}: {} setting{} applied{}.\n",
		entry->id.c_str(), entry->title.c_str(), entry->source.c_str(),
		applied, applied == 1 ? "" : "s",
		rejected ? G_Fmt(", {} rejected", rejected).data() : "");
}

void MM_Factory_ReapplyImmediate()
{
	using muffmode::gametype::cvar_owner_t;

	const mm_factory_t *entry = MM_Factory_Active();
	if (!entry)
		return;

	// Hand back the outgoing definition's overrides first. Without this, a
	// setting the host DELETED from the factory they just edited stays in force
	// -- the ledger still owns it, and the loop below only writes what the new
	// text names. Restoring makes a reload mean the edited file, not the union
	// of the edited file and the one before it.
	muffmode::gametype::MM_GT_RestoreOwned();

	int applied = 0;
	int deferred = 0;
	for (const parsed_override_t &override_entry : entry->overrides) {
		const mm_factory_cvar_t *known =
			MM_FindFactoryCvar(override_entry.name);
		// Map-load and latched settings cannot take effect mid-map. Say so
		// rather than writing a value that will not be observed until later.
		if (!known || known->apply != mm_factory_apply_t::Immediate) {
			deferred++;
			continue;
		}
		muffmode::gametype::MM_GT_WriteOwnedCvar(
			override_entry.name.c_str(), override_entry.value.c_str(),
			cvar_owner_t::Factory);
		applied++;
	}

	// The ruleset and the four scope directives are map-load work by nature:
	// the ruleset re-tunes the weapon tables at spawn, and a rotation only
	// matters at the next map pick. Counting them keeps the report honest --
	// "N applied" used to imply the whole factory had been re-applied.
	if (entry->ruleset != 0)
		deferred++;
	if (!entry->scope.Empty())
		deferred++;

	gi.Com_PrintFmt(
		"factories: {}: {} setting{} applied now, {} deferred to the next map "
		"load.{}\n",
		entry->id.c_str(), applied, applied == 1 ? "" : "s", deferred,
		deferred ? " Change map to apply the rest." : "");
}

} // namespace factory
} // namespace muffmode

// ---------------------------------------------------------------------------
// Player/admin command and votes.
// ---------------------------------------------------------------------------

namespace factory = muffmode::factory;
namespace session = muffmode::gametype;

namespace {

bool CallerIsAdmin(gentity_t *ent)
{
	return ent && ent->client && ent->client->sess.admin &&
		g_allow_admin && g_allow_admin->integer;
}

void PrintFactoryList(gentity_t *ent, gametype_t filter)
{
	const std::string list = factory::MM_Factory_List(filter);
	if (list.empty()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "No factories are available.\n");
		return;
	}
	gi.LocClient_Print(ent, PRINT_HIGH, "Available factories: {}\n",
		list.c_str());
}

// A map list too long for one console line, wrapped rather than truncated: a
// rotation a host cannot read in full is a rotation they cannot check.
void PrintTokenList(gentity_t *ent, const char *label,
	const std::vector<std::string> &tokens)
{
	if (tokens.empty())
		return;

	std::string line;
	for (const std::string &token : tokens) {
		if (!line.empty() && line.size() + 1 + token.size() > 60) {
			gi.LocClient_Print(ent, PRINT_HIGH, "  {} {}\n", label, line.c_str());
			line.clear();
			label = "        ";
		}
		if (!line.empty())
			line += ' ';
		line += token;
	}
	if (!line.empty())
		gi.LocClient_Print(ent, PRINT_HIGH, "  {} {}\n", label, line.c_str());
}

// `factory info` is documented as showing exactly what a preset changes, so it
// has to show the ruleset and the four directives too -- listing only the `set`
// lines hid the rotation, the map pool and the player limits, which is where
// most of the surprise in a shipped factory actually lives.
void PrintFactoryDetail(gentity_t *ent, const factory::mm_factory_t &entry)
{
	gi.LocClient_Print(ent, PRINT_HIGH, "{} - {} [{}] ({})\n",
		entry.id.c_str(), entry.title.c_str(),
		gt_short_name[(int)entry.base], entry.source.c_str());
	if (!entry.desc.empty())
		gi.LocClient_Print(ent, PRINT_HIGH, "  {}\n", entry.desc.c_str());

	if (entry.ruleset != 0)
		gi.LocClient_Print(ent, PRINT_HIGH, "  ruleset  {}\n",
			rs_long_name[entry.ruleset]);

	const factory::mm_factory_scope_t &scope = entry.scope;
	PrintTokenList(ent, "maps    ", scope.maps);
	PrintTokenList(ent, "mappool ", scope.mappool);
	if (scope.rotation != factory::mm_factory_rotation_t::Inherit)
		gi.LocClient_Print(ent, PRINT_HIGH, "  rotation {}\n",
			factory::MM_FactoryRotationName(scope.rotation));
	if (scope.min_players >= 0 && scope.max_players >= 0)
		gi.LocClient_Print(ent, PRINT_HIGH, "  players  {} - {}\n",
			scope.min_players, scope.max_players);

	for (const factory::parsed_override_t &item : entry.overrides)
		gi.LocClient_Print(ent, PRINT_HIGH, "  set {} {}\n",
			item.name.c_str(), item.value.c_str());

	if (!entry.Defines())
		gi.LocClient_Print(ent, PRINT_HIGH,
			"  (changes nothing; the plain gametype runs on your own config)\n");
}

// The allowlist is what an author needs in front of them while writing a
// factory, and 219 names is too many to page through blind -- so the prefix is
// the point of the command, not a garnish.
//
// Bounded on purpose. Every line here is a reliable message to one client, and
// the command is open to everyone: dumping all 219 names on request would let
// any player push ~55 reliable prints per invocation at whatever rate the flood
// protection allows. Without a prefix it prints the map, not the territory.
void PrintFactoryCvars(gentity_t *ent, const char *prefix)
{
	constexpr size_t k_max_lines = 12;
	constexpr size_t k_per_line = 4;

	if (!prefix || !*prefix) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"{} settings a factory may set. Narrow with a prefix, e.g.\n"
			"  factory cvars g_arena     factory cvars g_horde\n"
			"  factory cvars g_freezetag factory cvars g_dm\n"
			"  factory cvars g_vampiric  factory cvars limit\n"
			"`factory diag <cvar>` explains one. `factory info <id>` shows what "
			"a preset sets.\n", factory::k_factory_cvar_count);
		return;
	}

	const size_t prefix_len = strlen(prefix);
	std::string line;
	size_t matched = 0;
	size_t printed = 0;
	size_t lines = 0;
	size_t on_line = 0;
	for (size_t i = 0; i < factory::k_factory_cvar_count; i++) {
		const char *name = factory::k_factory_cvars[i].name;
		if (Q_strncasecmp(name, prefix, prefix_len) != 0)
			continue;

		matched++;
		if (lines >= k_max_lines)
			continue;

		if (on_line)
			line += ' ';
		line += name;
		if (++on_line == k_per_line) {
			gi.LocClient_Print(ent, PRINT_HIGH, "  {}\n", line.c_str());
			printed += on_line;
			line.clear();
			on_line = 0;
			lines++;
		}
	}
	if (on_line && lines < k_max_lines) {
		gi.LocClient_Print(ent, PRINT_HIGH, "  {}\n", line.c_str());
		printed += on_line;
	}

	if (!matched) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"No factory setting starts with \"{}\".\n", prefix);
		return;
	}
	if (printed < matched) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"{} of {} shown; narrow the prefix to see the rest.\n",
			printed, matched);
		return;
	}
	gi.LocClient_Print(ent, PRINT_HIGH,
		"{} of {} settings a factory may set.\n",
		matched, factory::k_factory_cvar_count);
}

void PrintCvarProvenance(gentity_t *ent, const char *name)
{
	// `factory diag` answers "where did this factory setting come from", and it
	// is open to every player. Restricting it to the allowlist is what keeps it
	// from being a general cvar reader: without this it happily prints
	// rcon_password, or any other server secret, to anyone who asks.
	//
	// The allowlist is lowercase and the lookup is a constexpr binary search, so
	// it cannot fold case itself. Cvar names are case-insensitive everywhere
	// else in the console, so fold here rather than telling a host that
	// `factory diag Fraglimit` is not a factory setting.
	std::string folded(name ? name : "");
	for (char &c : folded)
		if (c >= 'A' && c <= 'Z')
			c = (char)(c - 'A' + 'a');
	name = folded.c_str();

	const factory::mm_factory_cvar_t *known = factory::MM_FindFactoryCvar(name);
	if (!known) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"{} is not a factory setting. `factory diag` reports only cvars a "
			"factory may set; run `factory cvars` for the list.\n", name);
		return;
	}

	const cvar_t *cvar = gi.cvar(name, nullptr, CVAR_NOFLAGS);
	if (!cvar) {
		gi.LocClient_Print(ent, PRINT_HIGH, "No such cvar: {}\n", name);
		return;
	}

	const session::mm_cvar_provenance_t provenance =
		session::MM_GT_CvarProvenance(name);
	gi.LocClient_Print(ent, PRINT_HIGH, "{} = \"{}\"\n", name,
		cvar->string ? cvar->string : "");

	if (!provenance.tracked) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"  owner: server configuration (not overridden by the session)\n");
	} else {
		const char *owner =
			provenance.owner == session::cvar_owner_t::Factory ? "factory"
			: provenance.owner == session::cvar_owner_t::SessionAlias
				? "gametype session" : "none";
		gi.LocClient_Print(ent, PRINT_HIGH,
			"  owner: {}; applied \"{}\"; restores to \"{}\"{}\n",
			owner, provenance.applied_value.c_str(),
			provenance.prior_value.c_str(),
			provenance.drifted ? "; changed since by something else" : "");
	}

	const char *when =
		known->apply == factory::mm_factory_apply_t::Immediate ? "immediately"
		: known->apply == factory::mm_factory_apply_t::MapLoad
			? "at the next map load" : "at the next map load (latched)";
	gi.LocClient_Print(ent, PRINT_HIGH,
		"  a factory setting; a change takes effect {}\n", when);
}

// One selection path for the client command and the dedicated-server console
// alike, so a headless host cannot drift from what an in-game admin gets.
// `ent` may be null, in which case everything prints to the console.
void SelectFactory(gentity_t *ent, const char *id)
{
	// A null actor means the dedicated console, which outranks the lock.
	const session::gt_source_t source = ent
		? session::gt_source_t::Admin : session::gt_source_t::ServerConsole;

	if (Q_strcasecmp(id, "none") == 0 || Q_strcasecmp(id, "clear") == 0) {
		session::mm_gt_request_t request;
		request.source = source;
		request.clear_factory = true;
		const session::gt_reject_t reject = session::MM_GT_Submit(request);
		if (reject != session::gt_reject_t::None)
			gi.LocClient_Print(ent, PRINT_HIGH, "Cannot clear the factory: {}.\n",
				session::MM_GT_RejectText(reject));
		return;
	}

	const factory::mm_factory_t *entry = factory::MM_Factory_Find(id);
	if (!entry) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Unknown factory: {}\n", id);
		PrintFactoryList(ent, GT_NONE);
		return;
	}

	session::mm_gt_request_t request;
	request.source = source;
	request.has_factory = true;
	session::session_detail::CopyId(request.factory_id, entry->id.c_str());
	request.gametype = (int)entry->base;
	request.force_reapply = true;

	const session::gt_reject_t reject = session::MM_GT_Submit(request);
	if (reject != session::gt_reject_t::None)
		gi.LocClient_Print(ent, PRINT_HIGH, "Cannot select {}: {}.\n",
			entry->id.c_str(), session::MM_GT_RejectText(reject));
}

} // namespace

// factory                 -> show the active factory and the list
// factory list [gametype] -> list, optionally for one gametype
// factory info <id>       -> describe one factory
// factory diag <cvar>     -> where a setting's current value came from
// factory reload          -> rebuild the registry (admin)
// factory <id>            -> select it (admin)
void MM_CmdFactory(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	if (!deathmatch || !deathmatch->integer) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Factories are a deathmatch feature.\n");
		return;
	}

	const factory::mm_factory_t *active = factory::MM_Factory_Active();

	if (gi.argc() < 2) {
		if (active)
			gi.LocClient_Print(ent, PRINT_HIGH,
				"Current factory: {} ({}).\n", active->id.c_str(),
				active->title.c_str());
		else
			gi.LocClient_Print(ent, PRINT_HIGH,
				"No factory is active; the plain gametype is in force.\n");
		PrintFactoryList(ent, MM_CurrentGametype());
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Usage: factory <id> | list [gametype] | info <id> | cvars [prefix] | diag <cvar>\n");
		return;
	}

	const char *argument = gi.argv(1);

	if (Q_strcasecmp(argument, "list") == 0) {
		gametype_t filter = MM_CurrentGametype();
		if (gi.argc() >= 3) {
			if (Q_strcasecmp(gi.argv(2), "all") == 0) {
				filter = GT_NONE;
			} else {
				const mm_gt_lookup_t found = MM_GTFindByAnyName(gi.argv(2));
				// A retired gametype name still resolves -- `instagib` and
				// `nadefest` are kept in the table so ids do not renumber -- and
				// filtering on one would silently list nothing. Say why instead.
				if (found.found && MM_GetGametypeAvailability(found.gt) !=
					gametype_avail_t::Enabled) {
					gi.LocClient_Print(ent, PRINT_HIGH,
						"{} is not a selectable gametype. If you meant the "
						"modifier, it is a factory: try `factory {}`.\n",
						gi.argv(2), gi.argv(2));
					return;
				}
				if (!found.found) {
					gi.LocClient_Print(ent, PRINT_HIGH,
						"Unknown gametype: {}\n", gi.argv(2));
					return;
				}
				filter = found.gt;
			}
		}
		PrintFactoryList(ent, filter);
		return;
	}

	if (Q_strcasecmp(argument, "info") == 0) {
		if (gi.argc() != 3) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Usage: factory info <id>\n");
			return;
		}
		const factory::mm_factory_t *entry = factory::MM_Factory_Find(gi.argv(2));
		if (!entry) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Unknown factory: {}\n",
				gi.argv(2));
			return;
		}
		PrintFactoryDetail(ent, *entry);
		return;
	}

	if (Q_strcasecmp(argument, "diag") == 0) {
		if (gi.argc() != 3) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Usage: factory diag <cvar>\n");
			return;
		}
		PrintCvarProvenance(ent, gi.argv(2));
		return;
	}

	if (Q_strcasecmp(argument, "cvars") == 0) {
		PrintFactoryCvars(ent, gi.argc() >= 3 ? gi.argv(2) : nullptr);
		return;
	}

	// Everything past here mutates.
	if (!CallerIsAdmin(ent)) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Only admins can change the factory.\n");
		return;
	}

	if (Q_strcasecmp(argument, "reload") == 0) {
		// Reporting success unconditionally is worse than not reporting at all:
		// a host who mistyped an edit is told it reloaded, sees no change, and
		// has no reason to look at the console where the real error went.
		if (!factory::MM_Factory_LoadRegistry()) {
			gi.LocClient_Print(ent, PRINT_HIGH,
				"Factory registry reload FAILED; the previous registry is still "
				"in force. The console names the file, line and reason.\n");
			return;
		}
		factory::MM_Factory_ReapplyImmediate();
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Factory registry reloaded ({} factories).\n",
			factory::MM_Factory_Count());
		return;
	}

	SelectFactory(ent, argument);
}

// Same selection path, for a dedicated server console.
void MM_ServerSelectFactory(const char *id)
{
	SelectFactory(nullptr, id);
}
