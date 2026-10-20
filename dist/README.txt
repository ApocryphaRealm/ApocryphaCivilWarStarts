Alternate Perspective Civil War Starts
========================================
Version 1.0.7

An add-on for Alternate Perspective. It adds a "Civil War" card to the menu the Messenger offers you
in the Resting Pilgrim, with two starts under it:

  Stormcloak Recruit   the war room of the Palace of the Kings, Windhelm
  Imperial Recruit     the war room of Castle Dour, Solitude

Choose one and the game begins there, on the floor beside the war map where that side's command
stands, in heavy armour with a shield and a weapon, and with the game's own civil war recruitment
quest already running.
Galmar Stone-Fist or Legate Rikke is waiting to put you to the test, exactly as if you had walked in.

Nothing is skipped ahead. The mod starts the recruitment quest at its own opening stage, so the oath,
the officer's test and everything after it happen in game as they always do.

The uniform follows the armour mods you have installed - none of them is required. Which one is used is
decided by one file you can edit, SKSE\Plugins\CivilWarStarts_Kits.json (see ARMOUR MODS below).

  Stormcloak recruit   the first of these that is loaded:
                         Sons of Skyrim                   its Windhelm heavy set, with a Nord war axe
                         Guards Armor Replacer            its Eastmarch rebel set, with the Northerner axe
                         (none)                           the game's own Stormcloak gear
  Imperial recruit     the Legion set. New Legion and Heavy Legion both replace
                       the game's own Legion armour, so the start wears whichever of them you have installed.


WHAT IS IN THE PACKAGE
----------------------
  Alternate Perspective Civil War Starts.esp                             two quest records, nothing else
  SKSE\Plugins\CivilWarStarts.dll                the mod - the installer puts in the build for your game
  SKSE\Plugins\CivilWarStarts.pdb                debug symbols, so crash logs name this mod
  SKSE\Plugins\CivilWarStarts.ini                the settings, with every one explained
  SKSE\Plugins\CivilWarStarts_Kits.json         which armour mod each start wears, in the order they are tried
  SKSE\AlternatePerspective\CivilWarStarts.json  the starts, as Alternate Perspective reads them


REQUIREMENTS
------------
  SKSE64
  Address Library for SKSE Plugins
  Alternate Perspective - Alternate Start 4.0 or newer
  JContainers SE - Alternate Perspective's own requirement; without it AP shows only its own starts

  Supported runtimes: Skyrim SE 1.5.97, AE 1.6.1170, and Skyrim 1.7.99 / 1.7.104 - the installer asks
  which one you have and installs the matching DLL.


INSTALLATION
------------
  Install with a mod manager, pick your game version in the installer, and enable the plugin. Load
  order does not matter - nothing in this mod
  overrides anything.

  Nothing else needs setting up. The starts appear in Alternate Perspective's menu on the next new
  game.


SETTINGS
--------
  There is no in-game menu: the mod simply adds the two starts. The settings are in
  SKSE\Plugins\CivilWarStarts.ini, which explains each one where it sits. They apply to both
  sides:

    bEnabled               run the start at all
    fStartDelaySeconds     how long to wait after Alternate Perspective hands over
    bMoveToWarRoom         move to the war room
    bGiveStarterEquipment  hand over the uniform
    bEquipStarterEquipment wear it straight away
    bStartCivilWarQuest    start the recruitment questline
    uLogLevel              how much detail reaches the log


ARMOUR MODS
-----------
  CivilWarStarts_Kits.json lists, for each side, the armour sources in the order they are tried. A source
  is used when its plugin is loaded and every piece it needs is found under the name written in the file.
  If one is not, the log says why and the next source is tried; the last one is the game's own gear, so a start
  never hands out nothing. The comment at the top of the file explains the format.

  Supported out of the box:
    Sons of Skyrim (Nexus 68656)      Stormcloak
    Guards Armor Replacer (22671)     Stormcloak
    New Legion (30468)                Imperial - replaces the game's own Legion armour, so no entry is needed
    Heavy Legion (22877)              Imperial - the same
  A mod that only replaces the game's own armour, or only retextures it, works without an entry.

  To change the order, move a block up or down. To switch a mod off, delete its block. To add a mod, copy a
  block and change the plugin file name and the pieces (slot, FormID, name). Mark a piece "optional": true and
  the uniform goes without it when the mod has none - a mod with no shield works. If the file is missing or has
  a typo, the log gives the line and column and the lists built into the DLL are used.

IF SOMETHING GOES WRONG
-----------------------
  The log is at
  Documents\My Games\Skyrim Special Edition\SKSE\CivilWarStarts.log
  and it ships at info. For a bug report, set uLogLevel=0 in the INI, reproduce, and send the log.

  "I arrived back in the Resting Pilgrim."  Raise fStartDelaySeconds a little.
  "I ended up in Helgen."                   Lower it. Alternate Perspective has a safety net that
                                            moves you to the Helgen inn if a start takes too long,
                                            and it fired before this mod acted.
  "The starts are not in the menu."          Check that JContainers SE is installed - without it
                                            Alternate Perspective shows only its own starts.


WHAT CHANGED
------------
Version 1.0.7
One file, CivilWarStarts_Kits.json, decides which armour mod each start wears. The Stormcloak start takes its
uniform from Sons of Skyrim, then Guards Armor Replacer, then the game's own
gear; the Imperial start wears the Legion set that New Legion or Heavy Legion
give the game's own Legion armour. Each piece is checked by name first; if a plugin has changed, the next source
is used instead. Reorder, remove or add sources by editing the file.

Version 1.0.6
The log ships at info. For a bug report, set uLogLevel=0 in the INI.

Version 1.0.5
Adds a Skyrim 1.7.99 / 1.7.104 build. The download is now a FOMOD installer that asks which Skyrim you have and
installs the matching DLL (SE 1.5.97 / AE 1.6.1170 as before, or 1.7.x).
Removes the in-game menu page; every setting stays in the INI.
Names the other installer option when the wrong build is installed for the game, instead of asking for an
Address Library file that does not exist.


LICENCE
-------
  GPL-3.0-or-later. See LICENSE, NOTICE.md and THIRD_PARTY_NOTICES.md.
  Source: https://github.com/ApocryphaRealm/CivilWarStarts
