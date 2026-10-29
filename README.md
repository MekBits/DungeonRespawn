# ![logo](https://camo.githubusercontent.com/e4f036844d45424178f34d39dc3b0d52fdb82c07c5ef3c2aeb7e3da22c1538b3/68747470733a2f2f692e696d6775722e636f6d2f66517762386d332e706e67)Dungeon Respawn

## About

When a player dies in a dungeon or raid and releases, they come back alive at the dungeon's entrance instead of at the graveyard outside.

The entrance is the landing point of the instance portal (`areatrigger_teleport`) nearest to where the player died, so a dungeon with several entrances (Dire Maul, Scarlet Monastery, Blackrock Spire) uses the one closest to the body. Where you entered, or whether you were summoned in, does not matter. If the map has no portal into it, the player goes to the graveyard as usual.

This is a fork of [AnchyDev/DungeonRespawn](https://github.com/AnchyDev/DungeonRespawn), which is archived. The fork works with current AzerothCore and the playerbots core, and uses the respawn point above instead of the original's saved arrival point.

## Install

Just clone the module into your modules directory for your server, re-run cmake and compile.

Then enable the module in the module config.

## Contact

If you need any help you can join my [Discord Server](https://discord.gg/xdVPGcpJ8C).

## Credits

Thanks to Артём Борисов_99 from the AzerothCore Discord for the module suggestion.
