# Alternate Perspective Civil War Starts - changelog

## 1.0.0 - 2026-09-16 - untested

First version.

### Added

- **Two civil war starts for Alternate Perspective**, under one "Civil War" card in the Messenger's
  menu in the Resting Pilgrim:
  - **Stormcloak Recruit** - the war room of the Palace of the Kings, Windhelm.
  - **Imperial Recruit** - the war room of Castle Dour, Solitude.
- **The uniform.** Stormcloak: cuirass, boots, gauntlets, helmet and an iron sword, what the game's
  own `ArmorStormcloakOutfit` puts on a Stormcloak soldier. Imperial: light cuirass, boots, bracers,
  helmet, shield and an Imperial sword, what `CWSoldierImperialSoldierOutfitLight` puts on a legionary.
  Handed over and worn on arrival.
- **The questline running.** The game's own recruitment quest - `CW01B` "Joining the Stormcloaks" or
  `CW01A` "Joining the Legion" - is started at its own opening stage. Nothing is skipped ahead: the
  oath, the officer's test and everything after it happen in game exactly as they always do.
- **Every part of it optional**, from the mod's page in the Apocrypha Menu Framework or from its INI:
  the move, the uniform, whether it is worn, the questline, and how long the mod waits before acting.
- **A DevBench tool** (`civilwarstarts.start`) that reports what each side's start would do and can run
  either one without going through the menu.

### Notes

- Registration uses Alternate Perspective's own 4.0 JSON format, so no patch is needed on either side
  and nothing of AP's is edited or replaced.
- No vanilla record is edited or overridden. The plugin holds two quests and nothing else; the markers,
  armour and quests are the game's own, referenced at runtime.
- The player arrives at their side's **faction HQ marker**, not the map-table floor marker. The table
  marker sits underneath the war map, and the engine puts anyone moved there on top of the table.
