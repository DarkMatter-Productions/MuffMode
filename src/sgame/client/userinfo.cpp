// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "userinfo.h"

#include "muffmode/mm_ghost.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_player_name.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace muffmode::player {
namespace {

constexpr size_t kMaxPlayerStockSkins = 24;

struct StockModelSkins {
	const char *model;
	std::array<const char *, kMaxPlayerStockSkins> skins;
	size_t skin_count;
};

constexpr std::array<StockModelSkins, 3> kOriginalModels{{
	{
		"male",
		{{
			"grunt",
			"cipher", "claymore", "ctf_b",
			"ctf_r", "deaddude", "disguise",
			"flak", "howitzer", "insane1",
			"insane2", "insane3", "major",
			"nightops", "pointman", "psycho",
			"rampage", "razor", "recon",
			"rogue_b", "rogue_r", "scout",
			"sniper", "viper",
		}},
		24,
	},
	{
		"female",
		{{
			"athena",
			"brianna", "cobalt", "ctf_b",
			"ctf_r", "disguise", "ensign",
			"jezebel", "jungle", "lotus",
			"rogue_b", "rogue_r", "stiletto",
			"venus", "voodoo",
		}},
		15,
	},
	{
		"cyborg",
		{{
			"oni911",
			"ctf_b", "ctf_r", "disguise",
			"ps9000", "tyr574",
		}},
		6,
	},
}};

int32_t ParseUserinfoIntOrDefault(const char *value, int32_t default_value)
{
	const auto parsed = MM_ParseIntArg(value);
	return parsed ? *parsed : default_value;
}

int32_t ParseUserinfoClampedIntOrDefault(const char *value, int32_t default_value, int32_t min_value, int32_t max_value)
{
	return MM_ParseClampedIntArgOrDefault(value, default_value, min_value, max_value);
}

int32_t ParseUserinfoAutoShieldOrDefault(const char *value)
{
	return std::max(ParseUserinfoIntOrDefault(value, AUTO_SHIELD_MANUAL), AUTO_SHIELD_MANUAL);
}

bool ParseUserinfoBoolOrDefault(const char *value, bool default_value)
{
	const auto parsed = MM_ParseBoolArg(value);
	return parsed ? *parsed : default_value;
}

std::string EncodedPlayerName(gentity_t *player)
{
	return std::string("##P") + std::to_string(P_GetLobbyUserNum(player));
}

const char *SkinOverride(const char *skin)
{
	if (g_allow_custom_skins->integer)
		return skin;

	static char skin_buffer[MAX_QPATH];
	std::string_view requested_skin{ skin };
	std::string_view model = requested_skin;
	std::string_view skin_name = requested_skin;

	if (const auto separator = requested_skin.find('/'); separator != std::string_view::npos) {
		model = requested_skin.substr(0, separator);
		skin_name = requested_skin.substr(separator + 1);
	}

	if (model.empty()) {
		model = "male";
		skin_name = "grunt";
	}

	for (const auto &stock : kOriginalModels) {
		if (model != stock.model)
			continue;

		for (size_t i = 0; i < stock.skin_count; i++) {
			if (skin_name == stock.skins[i])
				return skin;
		}

		gi.Com_PrintFmt("{}: reverting to default skin for model: {} -> {}\n", __FUNCTION__, skin, stock.model, stock.skins[0]);
		G_FmtTo(skin_buffer, "{}/{}", stock.model, stock.skins[0]);
		return skin_buffer;
	}

	gi.Com_PrintFmt("{}: reverting to default model: {} -> male/grunt\n", __FUNCTION__, skin);
	Q_strlcpy(skin_buffer, "male/grunt", sizeof(skin_buffer));
	return skin_buffer;
}

} // namespace

float ParseUserinfoFov(const char *value)
{
	const std::string_view text = MM_TrimAsciiWhitespace(value ? std::string_view(value) : std::string_view());
	const auto parsed = MM_ParseFloatArg(text);
	if (!parsed)
		return kDefaultUserinfoFov;

	return clamp(*parsed, 1.0f, 160.0f);
}

void ApplyUserinfoChanged(
	gentity_t *ent, const char *userinfo,
	bool publish_skin_configstring, bool publish_name_configstring,
	bool refresh_skin_overrides)
{
	if (!ent || !userinfo || !g_entities || !game.clients ||
		game.maxclients <= 0 || !ent->client)
		return;
	const size_t client_index = MM_GhostAddressIndex(
		reinterpret_cast<uintptr_t>(ent),
		reinterpret_cast<uintptr_t>(&g_entities[1]), sizeof(g_entities[0]),
		static_cast<size_t>(game.maxclients));
	if (client_index == MM_GHOST_NO_CLIENT_INDEX ||
		ent->client != &game.clients[client_index])
		return;
	const bool is_bot = ent->client->sess.is_a_bot ||
		(ent->svflags & SVF_BOT);

	char val[MAX_INFO_VALUE] = {};

	if (!gi.Info_ValueForKey(userinfo, "name", ent->client->pers.netname, sizeof(ent->client->pers.netname)))
		Q_strlcpy(ent->client->pers.netname, "badinfo", sizeof(ent->client->pers.netname));

	if (is_bot) {
		const mm_bot_connection_name_t resolved_name =
			MM_PrepareBotUserinfoName(ent->client->pers.netname,
				ent->client->sess.bot_display_name,
				ent->client->sess.bot_base_name,
				bot_name_prefix->string);
		Q_strlcpy(ent->client->pers.netname,
			resolved_name.display_name.c_str(),
			sizeof(ent->client->pers.netname));
		Q_strlcpy(ent->client->sess.bot_base_name,
			resolved_name.base_name.c_str(),
			sizeof(ent->client->sess.bot_base_name));
		Q_strlcpy(ent->client->sess.bot_display_name,
			ent->client->pers.netname,
			sizeof(ent->client->sess.bot_display_name));
	} else {
		const std::string sanitized_name =
			MM_SanitizePlayerDisplayName(ent->client->pers.netname);
		Q_strlcpy(ent->client->pers.netname, sanitized_name.c_str(),
			sizeof(ent->client->pers.netname));
	}
	Q_strlcpy(ent->client->resp.netname, ent->client->pers.netname, sizeof(ent->client->resp.netname));

	if (!gi.Info_ValueForKey(userinfo, "skin", val, sizeof(val)))
		Q_strlcpy(val, "male/grunt", sizeof(val));

	Q_strlcpy(ent->client->pers.skin, SkinOverride(val), sizeof(ent->client->pers.skin));
	ent->client->pers.skin_icon_index = gi.imageindex(G_Fmt("/players/{}_i", ent->client->pers.skin).data());

	const int playernum = static_cast<int>(ent - g_entities - 1);

	if (publish_skin_configstring) {
		if (Teams() || GT(GT_ARENA)) {
			G_AssignPlayerSkin(ent, ent->client->pers.skin, refresh_skin_overrides);
		} else {
			gi.configstring(CS_PLAYERSKINS + playernum, G_Fmt("{}\\{}", ent->client->resp.netname, ent->client->pers.skin).data());
		}
	}
	if (publish_name_configstring)
		gi.configstring(CONFIG_FOLLOW_PLAYER_NAME + playernum,
			ent->client->resp.netname);

	if (MM_ShouldEncodePlayerName(is_bot)) {
		const auto encoded_name = EncodedPlayerName(ent);
		Q_strlcpy(ent->client->pers.netname, encoded_name.c_str(), sizeof(ent->client->pers.netname));
	}

	if (gi.Info_ValueForKey(userinfo, "fov", val, sizeof(val)))
		ent->client->ps.fov = ParseUserinfoFov(val);
	else
		ent->client->ps.fov = kDefaultUserinfoFov;

	if (gi.Info_ValueForKey(userinfo, "hand", val, sizeof(val))) {
		ent->client->pers.hand = static_cast<handedness_t>(
			ParseUserinfoClampedIntOrDefault(val, RIGHT_HANDED, RIGHT_HANDED, CENTER_HANDED_VISIBLE));
	} else {
		ent->client->pers.hand = RIGHT_HANDED;
	}

	if (gi.Info_ValueForKey(userinfo, "autoswitch", val, sizeof(val))) {
		ent->client->pers.autoswitch = static_cast<auto_switch_t>(
			ParseUserinfoClampedIntOrDefault(val, static_cast<int32_t>(auto_switch_t::SMART),
				static_cast<int32_t>(auto_switch_t::SMART), static_cast<int32_t>(auto_switch_t::NEVER)));
	} else {
		ent->client->pers.autoswitch = auto_switch_t::SMART;
	}

	if (gi.Info_ValueForKey(userinfo, "autoshield", val, sizeof(val)))
		ent->client->pers.autoshield = ParseUserinfoAutoShieldOrDefault(val);
	else
		ent->client->pers.autoshield = AUTO_SHIELD_MANUAL;

	if (gi.Info_ValueForKey(userinfo, "bobskip", val, sizeof(val)))
		ent->client->pers.bob_skip = ParseUserinfoBoolOrDefault(val, false);
	else
		ent->client->pers.bob_skip = false;

	Q_strlcpy(ent->client->pers.userinfo, userinfo, sizeof(ent->client->pers.userinfo));
	if (is_bot) {
		// Internal reparses must see the display value paired with bot_base_name.
		// Retaining an older/raw value here can turn it into the next base and
		// stack prefixes after a cvar change.
		gi.Info_SetValueForKey(ent->client->pers.userinfo, "name",
			ent->client->resp.netname);
	}
}

} // namespace muffmode::player

unsigned int P_GetLobbyUserNum(const gentity_t *player)
{
	unsigned int player_num = 0;
	if (player > g_entities && player < g_entities + MAX_ENTITIES) {
		player_num = static_cast<unsigned int>((player - g_entities) - 1);
		if (player_num >= MAX_CLIENTS)
			player_num = 0;
	}
	return player_num;
}

void ClientUserinfoChanged(gentity_t *ent, const char *userinfo)
{
	// Null-ent guard; same engine teardown class as ClientThink/ClientDisconnect.
	if (!ent)
		return;
	// A claimed/reinstating connection may update its parsed preferences, but its
	// canonical follow name, team skin, and per-viewer overrides are published
	// only by the bounded post-restore/abort presentation lane.
	const bool publish_immediately = !MM_Ghost_IsPendingRestore(ent);
	muffmode::player::ApplyUserinfoChanged(
		ent, userinfo, publish_immediately, publish_immediately,
		publish_immediately);
}

// [MuffMode] Ghost restore has already passed through the ordinary connection
// userinfo path. Parse the live connection's identity and preferences here
// without another reliable broadcast; the bounded post-restore lane publishes
// the canonical follow name and team skin before applying per-viewer
// corrections.
void ClientUserinfoChangedForRestore(gentity_t *ent, const char *userinfo)
{
	muffmode::player::ApplyUserinfoChanged(ent, userinfo, false, false, false);
}
