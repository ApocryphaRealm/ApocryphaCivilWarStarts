# Alternate Perspective Civil War Starts - changelog

## 1.0.3 - 2026-09-16 - untested

### Changed

- **The "Apocrypha" prefix is gone from the files** (the owner, 2026-09-16: *"Unprefix all of them.
  I don't care"*; framework mods keep theirs, this is not one). The DLL is `CivilWarStarts.dll`, so
  the INI is `CivilWarStarts.ini`, the log `CivilWarStarts.log` and the Alternate Perspective
  registration `SKSE\AlternatePerspective\CivilWarStarts.json`. Settings in an old
  `ApocryphaCivilWarStarts.ini` are not read; copy your values across once.
- **The plugin is an ESPFE** - `Alternate Perspective Civil War Starts.esp` carrying the light flag -
  instead of a `.esl` file (the owner's standing rule, 2026-09-16). A `.esl` file is forced to the top
  of the load order where nothing can sort below it; the `.esp` keeps the FE-space FormIDs and the
  freedom from the 254-slot limit, and sorts among ordinary plugins. Same two records, same FormIDs.
  Remove the old `.esl` when updating - a mod manager does this for you.

No behaviour change; the code is 1.0.2's with a new version stamp.

## 1.0.2 - 2026-09-16 - untested

### Fixed

- **The uniform can be taken off again.** It was being equipped with the engine's "force" flag, which is
  what Papyrus calls `abPreventRemoval` - it does not merely put an item on, it locks it on (the owner,
  2026-09-16: *"it wont let me unequip the items"*).

  The flag had been added to fix a helmet that appeared not to equip, and that was never a fault here:
  these starts all arrive **indoors**, and a mod that takes helmets off indoors was doing exactly what it
  is meant to do. Forcing the equip would have broken that mod rather than fixed anything. Each piece
  already gets its own frame after arrival, which is what actually makes the set go on reliably.

## 1.0.1 - 2026-09-16 - untested

### Changed

- **The start now waits until you have actually arrived before handing anything over, and the
  questline starts before the gear.** The owner, 2026-09-16: *"we should only get the items after
  teleporting"* and *"the quest should happen and then the equipment"*. The order is: teleport, wait
  until you are standing in the war room (checked, not timed - a slow load is waited out), start the
  recruitment quest, fill the pack, then wear the uniform a piece at a time.
- **The teleport is prompt.** The wait before it dropped from a second to a quarter of a second, and
  the beat between steps from 0.40s to 0.15s - a full second read as a long stare at black
  (*"it took too long to transport me"*). Raise `fStartDelaySeconds` if you ever land back in the
  Resting Pilgrim.
- **Fixed the kit from another mod coming up empty.** Every Sons of Skyrim piece reported "not in the
  game" while the plugin was plainly loaded: `TESDataHandler::LookupForm<T>` tests for an EXACT form
  type, so asking it for a `TESBoundObject` - a base class nothing actually is - returns null for
  every armour and weapon there is. It now uses the untemplated lookup and `As<>`.
- **Both sides now start in heavy armour, with a shield and a weapon.** The owner, 2026-09-16:
  *"i want to spawn with heavy armor and an axe and shield, all of which you can get from sons of
  skyrim mod"*.
  - **Stormcloak, with [Sons of Skyrim](https://www.nexusmods.com/skyrimspecialedition/mods/68656)
    installed:** Windhelm Heavy Armor, Windhelm Helmet, Windhelm Shield, Lamellar Heavy Boots and
    Gauntlets, and a Nord Heavy War Axe - the kit that mod gives Windhelm's own soldiers.
  - **Stormcloak, without it:** the game's own Stormcloak uniform, now with an iron shield and an
    iron war axe rather than a sword.
  - **Imperial:** the Legion's heavy set - Imperial armour, boots, gauntlets, helmet, shield and sword.

  Sons of Skyrim is **not a requirement**. Its pieces are looked up by plugin and local FormID at
  runtime, so the load order decides the real IDs, and if the plugin is not loaded the fallback kit is
  used instead. The log says which of the two it chose.

- **The start is spread over frames instead of happening in one.** The move, filling the pack, each
  piece of the uniform going on, and the questline starting are now separate steps a beat apart, set
  by the new `fStageGapSeconds` (0.40 by default, and a slider on the settings page).

  Why: on the first end-to-end run through Alternate Perspective's own menu, the game crashed three
  seconds after arrival. The crash was entirely inside Faster HDT-SMP - `hdt::CudaBody::Imp`, on one
  of its own worker threads, rebuilding collision bodies - with no frame of this mod anywhere in the
  stack, so the fault is not this mod's to fix. The *churn* is, though: a cross-cell teleport and a
  full set of armour going on in a single frame ask every mod that rebuilds itself from the player's
  body to do so at once. Spreading the work is gentler on whatever else is installed, and it is
  better behaved regardless. Set `fStageGapSeconds=0` to put it all back in one frame.

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
