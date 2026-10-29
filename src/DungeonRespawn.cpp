#include "DungeonRespawn.h"
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

    // The landing point of the instance portal nearest to the player. Teleports that start inside
    // the instance itself are skipped, so only entrances from outside count.
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

    // RepopAtGraveyard is also called for living players, e.g. when they fall below the map.
    if (!player || !player->isDead())
    {
        return true;
    }

    if (!IsInsideDungeonRaid(player))
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
