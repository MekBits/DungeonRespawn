#include "DungeonRespawn.h"
#include "AreaDefines.h"
#include "InstanceScript.h"
#include "ObjectMgr.h"

namespace
{
    bool drEnabled;
    float respawnHpPct;

    bool IsInsideDungeonRaid(Player* player)
    {
        Map* map = player->GetMap();
        if (!map)
        {
            return false;
        }

        return map->IsDungeon() || map->IsRaid();
    }

    // The Eye of Eternity's only entrance is on the platform Malygos destroys in phase 3, and drakes are only handed out once.
    bool IsEntranceUnsafe(Player* player)
    {
        if (player->GetMapId() != MAP_THE_EYE_OF_ETERNITY)
        {
            return false;
        }

        InstanceMap* instance = player->GetMap()->ToInstanceMap();
        InstanceScript* script = instance ? instance->GetInstanceScript() : nullptr;
        return script && script->IsEncounterInProgress();
    }

    // Teleports that start inside the instance are skipped, so only entrances from outside count.
    AreaTriggerTeleport const* GetNearestEntrance(Player* player)
    {
        AreaTriggerTeleport const* nearest = nullptr;
        float nearestDistSq = 0.0f;

        for (auto const& [triggerId, teleport] : sObjectMgr->GetAllAreaTriggerTeleports())
        {
            if (teleport.target_mapId != player->GetMapId())
            {
                continue;
            }

            AreaTrigger const* trigger = sObjectMgr->GetAreaTrigger(triggerId);
            if (!trigger || trigger->map == teleport.target_mapId)
            {
                continue;
            }

            float distSq = player->GetExactDistSq(teleport.target_X, teleport.target_Y, teleport.target_Z);
            if (!nearest || distSq < nearestDistSq)
            {
                nearest = &teleport;
                nearestDistSq = distSq;
            }
        }

        return nearest;
    }
}

bool DSPlayerScript::OnPlayerCanRepopAtGraveyard(Player* player)
{
    if (!drEnabled)
    {
        return true;
    }

    // Only a released ghost: the core also calls RepopAtGraveyard to eject living or unreleased players from an instance.
    if (!player || !player->HasPlayerFlag(PLAYER_FLAGS_GHOST))
    {
        return true;
    }

    if (!IsInsideDungeonRaid(player) || IsEntranceUnsafe(player))
    {
        return true;
    }

    // Without this check a vetoed resurrection would leave a ghost at the entrance with its corpse turned to bones.
    if (!sScriptMgr->OnPlayerCanResurrect(player))
    {
        return true;
    }

    AreaTriggerTeleport const* entrance = GetNearestEntrance(player);
    if (!entrance)
    {
        return true;
    }

    if (!player->TeleportTo(entrance->target_mapId, entrance->target_X, entrance->target_Y, entrance->target_Z, entrance->target_Orientation))
    {
        return true;
    }

    player->ResurrectPlayer(respawnHpPct / 100.0f, false);
    player->SpawnCorpseBones();
    // RepopAtGraveyard clears this after its own teleport, and returning false skips that.
    player->RemovePlayerFlag(PLAYER_FLAGS_IS_OUT_OF_BOUNDS);
    return false;
}

void DSWorldScript::OnAfterConfigLoad(bool /*reload*/)
{
    drEnabled = sConfigMgr->GetOption<bool>("DungeonRespawn.Enable", false);
    respawnHpPct = sConfigMgr->GetOption<float>("DungeonRespawn.RespawnHealthPct", 50.0f);
}

void SC_AddDungeonRespawnScripts()
{
    new DSWorldScript();
    new DSPlayerScript();
}
