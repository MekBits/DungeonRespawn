#ifndef MODULE_DUNGEONRESPAWN_H
#define MODULE_DUNGEONRESPAWN_H

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"

class DSPlayerScript : public PlayerScript
{
public:
    DSPlayerScript() : PlayerScript("DSPlayerScript", {
        PLAYERHOOK_CAN_REPOP_AT_GRAVEYARD
    }) { }

private:
    bool OnPlayerCanRepopAtGraveyard(Player* /*player*/) override;
};

class DSWorldScript : public WorldScript
{
public:
    DSWorldScript() : WorldScript("DSWorldScript", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD
    }) { }

private:
    void OnAfterConfigLoad(bool /*reload*/) override;
};

#endif //MODULE_DUNGEONRESPAWN_H
