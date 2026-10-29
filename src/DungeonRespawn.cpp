#include "DungeonRespawn.h"
#include <mutex>

namespace
{
    std::vector<PlayerRespawnData> respawnData;
    // Player hooks run on the map update threads, so respawnData and playersToTeleport are shared.
    std::mutex respawnLock;

    bool drEnabled;
    float respawnHpPct;
}

bool DSPlayerScript::IsInsideDungeonRaid(Player* player)
{
    if (!player)
    {
        return false;
    }

    Map* map = player->GetMap();
    if (!map)
    {
        return false;
    }

    if (!map->IsDungeon() && !map->IsRaid())
    {
        return false;
    }

    return true;
}
void DSPlayerScript::OnPlayerReleasedGhost(Player* player)
{
    if (!drEnabled)
    {
        return;
    }

    if (!IsInsideDungeonRaid(player))
    {
        return;
    }

    std::lock_guard<std::mutex> guard(respawnLock);
    playersToTeleport.push_back(player->GetGUID());
}

void DSPlayerScript::ResurrectPlayer(Player* player)
{
    player->ResurrectPlayer(respawnHpPct / 100.0f, false);
    player->SpawnCorpseBones();
}

bool DSPlayerScript::OnPlayerBeforeTeleport(Player* player, uint32 mapid, float /*x*/, float /*y*/, float /*z*/, float /*orientation*/, uint32 /*options*/, Unit* /*target*/)
{
    if (!drEnabled)
    {
        return true;
    }

    if (!player)
    {
        return true;
    }

    std::unique_lock<std::mutex> guard(respawnLock);

    if (player->GetMapId() != mapid)
    {
        auto prData = GetOrCreateRespawnData(player);
        prData->isTeleportingNewMap = true;
    }

    if (!IsInsideDungeonRaid(player))
    {
        return true;
    }

    if (!player->isDead())
    {
        return true;
    }

    GuidVector::iterator itToRemove;
    bool canRestore = false;

    for (auto it = playersToTeleport.begin(); it != playersToTeleport.end(); ++it)
    {
        if (*it == player->GetGUID())
        {
            itToRemove = it;
            canRestore = true;
            break;
        }
    }

    if (!canRestore)
    {
        return true;
    }

    playersToTeleport.erase(itToRemove);

    auto prData = GetOrCreateRespawnData(player);
    if (prData)
    {
        //Invalid Player Restore data, use default behaviour.
        if (prData->dungeon.map == -1)
        {
            return true;
        }

        if (prData->dungeon.map != int32(player->GetMapId()))
        {
            return true;
        }

        DungeonData dungeon = prData->dungeon;

        // TeleportTo calls this hook again, so the lock must be released first.
        guard.unlock();
        player->TeleportTo(dungeon.map, dungeon.x, dungeon.y, dungeon.z, dungeon.o);
        ResurrectPlayer(player);
        return false;
    }

    return true;
}

void DSWorldScript::OnAfterConfigLoad(bool reload)
{
    std::lock_guard<std::mutex> guard(respawnLock);

    if (reload)
    {
        SaveRespawnData();
        respawnData.clear();
    }

    drEnabled = sConfigMgr->GetOption<bool>("DungeonRespawn.Enable", false);
    respawnHpPct = sConfigMgr->GetOption<float>("DungeonRespawn.RespawnHealthPct", 50.0f);

    QueryResult qResult = CharacterDatabase.Query("SELECT `guid`, `map`, `x`, `y`, `z`, `o` FROM `dungeonrespawn_playerinfo`");

    if (qResult)
    {
        uint32 dataCount = 0;

        do
        {
            Field* fields = qResult->Fetch();

            PlayerRespawnData prData;
            DungeonData dData;
            prData.guid = ObjectGuid(fields[0].Get<uint64>());
            dData.map = fields[1].Get<int32>();
            dData.x = fields[2].Get<float>();
            dData.y = fields[3].Get<float>();
            dData.z = fields[4].Get<float>();
            dData.o = fields[5].Get<float>();
            prData.dungeon = dData;
            prData.isTeleportingNewMap = false;
            prData.inDungeon = false;

            respawnData.push_back(prData);

            dataCount++;
        } while (qResult->NextRow());

        LOG_INFO("module", "Loaded '{}' rows from 'dungeonrespawn_playerinfo' table.", dataCount);
    }
    else
    {
        LOG_INFO("module", "Loaded '0' rows from 'dungeonrespawn_playerinfo' table.");
        return;
    }
}

void DSWorldScript::OnShutdown()
{
    std::lock_guard<std::mutex> guard(respawnLock);
    SaveRespawnData();
}

void DSWorldScript::SaveRespawnData()
{
    for (const auto& prData : respawnData)
    {
        if (prData.inDungeon)
        {
            CharacterDatabase.Execute("INSERT INTO `dungeonrespawn_playerinfo` (guid, map, x, y, z, o) VALUES ({}, {}, {}, {}, {}, {}) ON DUPLICATE KEY UPDATE map={}, x={}, y={}, z={}, o={}",
                prData.guid.GetRawValue(),
                prData.dungeon.map,
                prData.dungeon.x,
                prData.dungeon.y,
                prData.dungeon.z,
                prData.dungeon.o,
                prData.dungeon.map,
                prData.dungeon.x,
                prData.dungeon.y,
                prData.dungeon.z,
                prData.dungeon.o);
        }
        else
        {
            CharacterDatabase.Execute("DELETE FROM `dungeonrespawn_playerinfo` WHERE guid = {}", prData.guid.GetRawValue());
        }
    }
}

PlayerRespawnData* DSPlayerScript::GetOrCreateRespawnData(Player* player)
{
    for (auto it = respawnData.begin(); it != respawnData.end(); ++it)
    {
        if (it != respawnData.end())
        {
            if (player->GetGUID() == it->guid)
            {
                return &(*it);
            }
        }
    }

    CreateRespawnData(player);

    return GetOrCreateRespawnData(player);
}

void DSPlayerScript::OnPlayerMapChanged(Player* player)
{
    if (!player)
    {
        return;
    }

    std::lock_guard<std::mutex> guard(respawnLock);
    auto prData = GetOrCreateRespawnData(player);

    if (!prData)
    {
        return;
    }

    bool inDungeon = IsInsideDungeonRaid(player);
    prData->inDungeon = inDungeon;

    if (!inDungeon)
    {
        return;
    }

    if (!prData->isTeleportingNewMap)
    {
        return;
    }

    prData->dungeon.map = player->GetMapId();
    prData->dungeon.x = player->GetPositionX();
    prData->dungeon.y = player->GetPositionY();
    prData->dungeon.z = player->GetPositionZ();
    prData->dungeon.o = player->GetOrientation();

    prData->isTeleportingNewMap = false;
}

void DSPlayerScript::CreateRespawnData(Player* player)
{
    DungeonData newDData;
    newDData.map = -1;
    newDData.x = 0;
    newDData.y = 0;
    newDData.z = 0;
    newDData.o = 0;

    PlayerRespawnData newPrData;
    newPrData.dungeon = newDData;
    newPrData.guid = player->GetGUID();
    newPrData.isTeleportingNewMap = false;
    newPrData.inDungeon = false;
    
    respawnData.push_back(newPrData);
}

void DSPlayerScript::OnPlayerLogin(Player* player)
{
    if (!player)
    {
        return;
    }

    std::lock_guard<std::mutex> guard(respawnLock);
    GetOrCreateRespawnData(player);
}

void DSPlayerScript::OnPlayerLogout(Player* player)
{
    if (!player)
    {
        return;
    }

    std::lock_guard<std::mutex> guard(respawnLock);
    std::erase(playersToTeleport, player->GetGUID());
}

void SC_AddDungeonRespawnScripts()
{
    new DSWorldScript();
    new DSPlayerScript();
}
