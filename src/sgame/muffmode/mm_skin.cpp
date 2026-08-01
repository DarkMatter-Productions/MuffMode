// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_message_budget.h"
#include "muffmode/mm_pconfig.h"
#include "muffmode/mm_pconfig_rules.h"
#include "muffmode/mm_skin.h"

#include <cstring>

//=======================================================================
// PER-VIEWER TEAM/ENEMY SKIN OVERRIDES
//
// Lets each player re-skin the enemies and teammates they see, locally, via
// the eskin/tskin commands. Overrides are unicast to the individual viewer as
// CS_PLAYERSKINS configstrings, so other players' views are untouched. Because
// any canonical CS_PLAYERSKINS broadcast (skin assignment, team change, spawn)
// clobbers a viewer's override, the refresh entry points re-send overrides
// after those events.
//=======================================================================

namespace {

enum class skin_relationship_t {
	None,
	Teammate,
	Enemy
};

bool MM_IsClientEntity(gentity_t *ent) {
	return ent && ent->inuse && ent->client && ent->client->pers.connected;
}

bool MM_IsSkinClientReady(gentity_t *ent) {
	return MM_IsClientEntity(ent) && ent->client->pers.spawned;
}

size_t MM_PlayerSkinConfigStringLength(const char *netname, const char *skin) {
	return std::strlen(netname) + 1 + std::strlen(skin) + 1 + std::strlen("default");
}

bool MM_PlayerSkinConfigStringFits(const char *netname, const char *skin, int32_t playernum) {
	return MM_PlayerSkinConfigStringLength(netname, skin) < CS_SIZE(CS_PLAYERSKINS + playernum);
}

bool MM_PlayerSkinCanBeStored(const char *skin) {
	return skin && muffmode::pconfig::IsStorableSkinPath(skin, MAX_QPATH, MAX_NETNAME - 1, CS_SIZE(CS_PLAYERSKINS));
}

bool MM_BuildSkinOverrideConfigString(gentity_t *target, const char *skin, char (&buffer)[CS_MAX_STRING_LENGTH]) {
	if (!MM_IsClientEntity(target) || !skin || !*skin)
		return false;
	if (!MM_PlayerSkinCanBeStored(skin))
		return false;

	const int32_t playernum = target - g_entities - 1;
	if (playernum < 0 || playernum >= static_cast<int32_t>(game.maxclients))
		return false;

	if (!MM_PlayerSkinConfigStringFits(target->client->resp.netname, skin, playernum))
		return false;

	G_FmtTo(buffer, "{}\\{}\\default", target->client->resp.netname, skin);
	return true;
}

bool MM_SendPlayerSkinConfigString(gentity_t *viewer, int32_t playernum, const char *value) {
	if (!MM_IsSkinClientReady(viewer) || !value)
		return false;
	if (playernum < 0 || playernum >= static_cast<int32_t>(game.maxclients))
		return false;

	// Reserve the entire protocol message before the first write. memchr keeps a
	// malformed canonical value from turning the accounting itself into an
	// unbounded read; valid player configstrings include a NUL within this span.
	const void *terminator = std::memchr(value, '\0', CS_MAX_STRING_LENGTH);
	if (!terminator)
		return false;
	const size_t value_bytes = static_cast<size_t>(
		static_cast<const char *>(terminator) - value) + 1;
	constexpr size_t CONFIGSTRING_MESSAGE_HEADER_BYTES = 1 + 2;
	if (!MM_ReserveReliableFanoutMessage(
			CONFIGSTRING_MESSAGE_HEADER_BYTES + value_bytes))
		return false;

	gi.WriteByte(svc_configstring);
	gi.WriteShort(CS_PLAYERSKINS + playernum);
	gi.WriteString(value);
	gi.unicast(viewer, true);
	return true;
}

bool MM_SendCanonicalPlayerSkin(gentity_t *viewer, gentity_t *target) {
	const int32_t playernum = target - g_entities - 1;

	if (playernum < 0 || playernum >= static_cast<int32_t>(game.maxclients))
		return false;

	return MM_SendPlayerSkinConfigString(viewer, playernum, gi.get_configstring(CS_PLAYERSKINS + playernum));
}

// [MuffMode] Overrides live in the player's session config (sess.pc), matching
// the rest of the MuffMode preference system, instead of raw userinfo keys.
const char *MM_StoredSkinOverride(gentity_t *viewer, bool is_enemy) {
	if (!MM_IsClientEntity(viewer))
		return "";

	return is_enemy ? viewer->client->sess.pc.enemy_skin : viewer->client->sess.pc.team_skin;
}

gentity_t *MM_EffectiveSkinOverrideViewer(gentity_t *viewer) {
	if (!MM_IsClientEntity(viewer))
		return nullptr;

	if (ClientIsPlaying(viewer->client))
		return viewer;

	gentity_t *follow_target = viewer->client->follow_target;
	if (MM_IsClientEntity(follow_target) && ClientIsPlaying(follow_target->client))
		return follow_target;

	return nullptr;
}

bool MM_SkinOverridesEnabled() {
	return Teams() || GT(GT_DUEL) || GT(GT_ARENA);
}

skin_relationship_t MM_SkinOverrideRelationship(gentity_t *perspective, gentity_t *target) {
	if (!MM_IsClientEntity(perspective) || !MM_IsClientEntity(target))
		return skin_relationship_t::None;

	if (GT(GT_DUEL))
		return perspective != target &&
			ClientIsPlaying(perspective->client) &&
			ClientIsPlaying(target->client) ?
			skin_relationship_t::Enemy : skin_relationship_t::None;

	if (GT(GT_ARENA)) {
		// Arena sides are a transient red/blue projection. Skin relationships
		// follow the persistent logical team, and only exist between fighters
		// in the same room; observers, coaches, queued players, and fighters in
		// other arenas are unrelated for per-viewer overrides.
		if (!MM_Arena_IsFighter(perspective->client) ||
			!MM_Arena_IsFighter(target->client) ||
			MM_Arena_Id(perspective) <= 0 ||
			MM_Arena_Id(target) <= 0 ||
			!MM_Arena_SameArena(perspective, target))
			return skin_relationship_t::None;

		return MM_Arena_SameTeam(perspective, target) ?
			skin_relationship_t::Teammate : skin_relationship_t::Enemy;
	}

	if (!Teams())
		return skin_relationship_t::None;

	return perspective->client->sess.team == target->client->sess.team ?
		skin_relationship_t::Teammate : skin_relationship_t::Enemy;
}

bool MM_ShouldOverrideTarget(gentity_t *viewer, gentity_t *target, const char **skin) {
	*skin = nullptr;

	if (!g_allow_skin_overrides->integer)
		return false;

	// Team relationship is judged from the perspective being viewed (the player
	// itself, or its follow target while spectating), but the stored preference
	// always belongs to the real viewer entity.
	gentity_t *team_viewer = MM_EffectiveSkinOverrideViewer(viewer);

	if (!MM_SkinOverridesEnabled() || !team_viewer || !MM_IsClientEntity(target))
		return false;
	if (team_viewer == target || !ClientIsPlaying(target->client))
		return false;

	const skin_relationship_t relationship =
		MM_SkinOverrideRelationship(team_viewer, target);
	if (relationship == skin_relationship_t::None)
		return false;

	*skin = MM_StoredSkinOverride(viewer,
		relationship == skin_relationship_t::Enemy);

	return **skin != 0;
}

bool MM_SendSkinOverride(gentity_t *viewer, gentity_t *target) {
	const char *skin = nullptr;

	if (!MM_ShouldOverrideTarget(viewer, target, &skin))
		return false;

	const int32_t playernum = target - g_entities - 1;
	char config[CS_MAX_STRING_LENGTH] = {};

	if (MM_BuildSkinOverrideConfigString(target, skin, config))
		return MM_SendPlayerSkinConfigString(viewer, playernum, config);

	return false;
}

void MM_SendEffectivePlayerSkin(gentity_t *viewer, gentity_t *target) {
	// Exactly one value wins for a viewer/target pair. Sending the canonical
	// value and then immediately replacing it doubled reliable reconnect traffic.
	if (!MM_SendSkinOverride(viewer, target))
		MM_SendCanonicalPlayerSkin(viewer, target);
}

void MM_CmdSkinOverride(gentity_t *ent, bool is_enemy, const char *label, const char *affected_players, const char *command) {
	if (!MM_IsClientEntity(ent))
		return;

	if (!g_allow_skin_overrides->integer) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Skin overrides are disabled on this server.\n");
		return;
	}

	if (!is_enemy && GT(GT_DUEL)) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"tskin is not available in duel. Use eskin to re-skin your opponent on your screen.\n");
		return;
	}

	char *store = is_enemy ? ent->client->sess.pc.enemy_skin : ent->client->sess.pc.team_skin;

	if (gi.argc() < 2) {
		if (*store)
			gi.LocClient_Print(ent, PRINT_HIGH,
				"{} skin override is '{}'.\n"
				"Usage: {} <model/skin> (example: {} male/grunt) or {} off. Only your view changes; {} use this skin for you.\n",
				label, store, command, command, command, affected_players);
		else
			gi.LocClient_Print(ent, PRINT_HIGH,
				"{} skin override is off.\n"
				"Usage: {} <model/skin> (example: {} male/grunt) or {} off. Only your view changes; {} will use the chosen skin for you.\n",
				label, command, command, command, affected_players);
		return;
	}

	if (gi.argc() > 2) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Usage: {} <model/skin> (example: {} male/grunt) or {} off. A skin path is a single token with no spaces.\n",
			command, command, command);
		return;
	}

	const char *skin = gi.argv(1);

	if (muffmode::pconfig::IsDisableToken(skin)) {
		store[0] = 0;
		MM_ClientSavePConfigOrWarn(ent);
		MM_RefreshSkinOverridesForViewer(ent);
		gi.LocClient_Print(ent, PRINT_HIGH, "{} skin override cleared. Only your view changes; {} now use their normal skins for you.\n", label, affected_players);
		return;
	}

	if (!muffmode::pconfig::IsSafeSkinPath(skin)) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Usage: {} <model/skin> (example: {} male/grunt) or {} off. Only your view changes; {} are affected.\n"
			"Skin paths may use letters, numbers, '_', '-', and '/'.\n",
			command, command, command, affected_players);
		return;
	}

	if (!MM_PlayerSkinCanBeStored(skin)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "{} is too long for player skin configstrings.\n", skin);
		return;
	}

	Q_strlcpy(store, skin, MAX_QPATH);
	MM_ClientSavePConfigOrWarn(ent);
	MM_RefreshSkinOverridesForViewer(ent);
	gi.LocClient_Print(ent, PRINT_HIGH, "{} skin override set to '{}'. Only your view changes; {} will use that skin for you.\n", label, skin, affected_players);
}

} // namespace

bool MM_PublishCanonicalPlayerSkin(gentity_t *target) {
	if (!MM_IsClientEntity(target))
		return false;

	const int32_t playernum = target - g_entities - 1;
	if (playernum < 0 || playernum >= static_cast<int32_t>(game.maxclients))
		return false;

	const std::string_view value = Teams() || GT(GT_ARENA) ?
		G_FormatPlayerSkinConfigString(target, target->client->pers.skin) :
		G_Fmt("{}\\{}", target->client->resp.netname, target->client->pers.skin);
	if (value.size() >= CS_SIZE(CS_PLAYERSKINS + playernum))
		return false;

	constexpr size_t CONFIGSTRING_MESSAGE_HEADER_BYTES = 1 + 2;
	if (!MM_ReserveReliableFanoutMessage(
			CONFIGSTRING_MESSAGE_HEADER_BYTES + value.size() + 1))
		return false;

	gi.configstring(CS_PLAYERSKINS + playernum, value.data());
	return true;
}

void MM_CmdEnemySkin(gentity_t *ent) {
	if (GT(GT_DUEL))
		MM_CmdSkinOverride(ent, true, "Opponent", "your opponent", "eskin");
	else
		MM_CmdSkinOverride(ent, true, "Enemy", "enemies", "eskin");
}

void MM_CmdTeamSkin(gentity_t *ent) {
	MM_CmdSkinOverride(ent, false, "Team", "teammates", "tskin");
}

void MM_RefreshSkinOverridesForTarget(gentity_t *target) {
	if (!MM_IsSkinClientReady(target))
		return;

	for (auto viewer : active_clients()) {
		if (viewer == target)
			continue;

		// G_AssignPlayerSkin has just broadcast the canonical value. Only viewers
		// that actually override this target need a correcting unicast.
		MM_SendSkinOverride(viewer, target);
	}
}

void MM_RefreshSkinOverridesForViewer(gentity_t *viewer) {
	if (!MM_IsSkinClientReady(viewer))
		return;

	for (auto target : active_clients()) {
		if (target == viewer)
			continue;

		MM_SendEffectivePlayerSkin(viewer, target);
	}
}

bool MM_ReapplySkinOverride(gentity_t *viewer, gentity_t *target) {
	if (!MM_IsSkinClientReady(viewer) || !MM_IsSkinClientReady(target) || viewer == target)
		return false;

	return MM_SendSkinOverride(viewer, target);
}
