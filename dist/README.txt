Alternate Perspective Civil War Starts
========================================

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


WHAT IS IN THE PACKAGE
----------------------
  Alternate Perspective Civil War Starts.esp                             two quest records, nothing else
  SKSE\Plugins\ApocryphaCivilWarStarts.dll                the mod
  SKSE\Plugins\ApocryphaCivilWarStarts.pdb                debug symbols, so crash logs name this mod
  SKSE\Plugins\ApocryphaCivilWarStarts.ini                the settings, with every one explained
  SKSE\AlternatePerspective\ApocryphaCivilWarStarts.json  the starts, as Alternate Perspective reads them


REQUIREMENTS
------------
  SKSE64
  Address Library for SKSE Plugins
  Alternate Perspective - Alternate Start 4.0 or newer
  JContainers SE - Alternate Perspective's own requirement; without it AP shows only its own starts

  Supported runtimes: Skyrim SE 1.5.97 and AE 1.6.1170.

  Optional: the Apocrypha Menu Framework (or SKSE Menu Framework), which is where this mod's settings
  page appears. Without one the mod reads its INI and works the same; it simply has no page.


INSTALLATION
------------
  Install with a mod manager and enable the plugin. Load order does not matter - nothing in this mod
  overrides anything.

  Nothing else needs setting up. The starts appear in Alternate Perspective's menu on the next new
  game.


SETTINGS
--------
  Everything is on this mod's page in the Apocrypha Menu Framework, and in
  SKSE\Plugins\ApocryphaCivilWarStarts.ini, which explains each one where it sits. They apply to both
  sides:

    bEnabled               run the start at all
    fStartDelaySeconds     how long to wait after Alternate Perspective hands over
    bMoveToWarRoom         move to the war room
    bGiveStarterEquipment  hand over the uniform
    bEquipStarterEquipment wear it straight away
    bStartCivilWarQuest    start the recruitment questline
    uLogLevel              how much detail reaches the log


IF SOMETHING GOES WRONG
-----------------------
  The log is at
  Documents\My Games\Skyrim Special Edition\SKSE\ApocryphaCivilWarStarts.log
  and it ships at full detail, so it already contains everything needed.

  "I arrived back in the Resting Pilgrim."  Raise fStartDelaySeconds a little.
  "I ended up in Helgen."                   Lower it. Alternate Perspective has a safety net that
                                            moves you to the Helgen inn if a start takes too long,
                                            and it fired before this mod acted.
  "The starts are not in the menu."          Check that JContainers SE is installed - without it
                                            Alternate Perspective shows only its own starts.


LICENCE
-------
  GPL-3.0-or-later. See LICENSE, NOTICE.md and THIRD_PARTY_NOTICES.md.
  Source: https://github.com/ApocryphaRealm/ApocryphaCivilWarStarts
