from __future__ import annotations

import argparse
import re
import shutil
import struct
from collections import Counter, defaultdict
from pathlib import Path


WEAPON_NAMES = {
    "weapon_bfg": "BFG10K",
    "weapon_chaingun": "Chaingun",
    "weapon_grenadelauncher": "Grenade Launcher",
    "weapon_hyperblaster": "HyperBlaster",
    "weapon_machinegun": "Machinegun",
    "weapon_plasmabeam": "Plasma Beam",
    "weapon_railgun": "Railgun",
    "weapon_rocketlauncher": "Rocket Launcher",
    "weapon_shotgun": "Shotgun",
    "weapon_supershotgun": "Super Shotgun",
}

AMMO_NAMES = {
    "ammo_bullets": "Bullets",
    "ammo_bullets_large": "Bullets (large)",
    "ammo_cells": "Cells",
    "ammo_cells_large": "Cells (large)",
    "ammo_cells_small": "Cells (small)",
    "ammo_grenades": "Grenades",
    "ammo_rockets": "Rockets",
    "ammo_rockets_small": "Rockets (small)",
    "ammo_shells": "Shells",
    "ammo_shells_large": "Shells (large)",
    "ammo_slugs": "Slugs",
    "ammo_slugs_small": "Slugs (small)",
}

UTILITY_NAMES = {
    "item_adrenaline": "Adrenaline",
    "item_armor_body": "Body Armor",
    "item_armor_combat": "Combat Armor",
    "item_armor_jacket": "Jacket Armor",
    "item_armor_shard": "Armor Shard",
    "item_bandolier": "Bandolier",
    "item_breather": "Rebreather",
    "item_double": "Double Damage / Haste",
    "item_health": "Health",
    "item_health_large": "Large Health",
    "item_health_mega": "Mega Health",
    "item_health_small": "Small Health",
    "item_pack": "Ammo Pack",
    "item_power_screen": "Power Screen",
    "item_power_shield": "Power Shield",
    "item_quad": "Quad Damage",
}

SPAWN_NAMES = {
    "info_player_deathmatch": "Deathmatch spawn",
    "info_player_intermission": "Intermission camera",
    "info_player_start": "Start point",
}


FINAL_MAPS = [
    {
        "file": "mm-aerow",
        "title": "Aerowalk",
        "original": "aeroq2 / Aerowalk",
        "original_author": 'Mattias "Preacher" Konradsson',
        "release_date": "11 April 1998",
        "players": "2-4",
        "gametypes": ["Duel", "small FFA", "2v2", "Clan Arena"],
        "readme": "aerowalk-readme.txt",
        "bsp": "aerowalk.bsp",
        "source_urls": [("DondeQ2 map archive", "https://dondeq2.com/2017/10/24/webman-twists-map-collection/")],
        "summary": "A compact pressure cooker built around fast vertical returns, clean rocket fights, and constant Mega Health tension.",
        "history": "Preacher's Quake II Aerowalk adaptation carried one of deathmatch's most famous duel layouts into the Strogg era. The Muff Mode version keeps the immediate read of the original while rebuilding it for the rerelease toolchain.",
        "play": "Best when the lobby wants a map that rewards timing, denial, and quick escapes. With six starts it can support a small FFA, but it shines brightest when two players are fighting over the central routes.",
    },
    {
        "file": "mm-biorust",
        "title": "Bio Rust",
        "original": "koldduel1 / Bio Rust",
        "original_author": "kold",
        "release_date": "12 October 2008",
        "players": "2-4",
        "gametypes": ["Duel", "small FFA", "2v2"],
        "readme": None,
        "bsp": "koldduel1.bsp",
        "source_urls": [("quake2.com.ru listing", "https://quake2.com.ru/files/maps/2/")],
        "summary": "A duel-leaning bio-industrial arena with enough armor and ammo density to keep fights moving between tight rooms and exposed transitions.",
        "history": "Bio Rust entered the later Quake II duel pool as koldduel1. Compared with the 1998 classics, it feels more modern in its item pacing and its willingness to make players re-fight the same junctions from different angles.",
        "play": "Use it for focused duel or small-session FFA. Two Combat Armors and a Mega Health make timing matter, while two Super Shotguns keep the map hostile at close range.",
    },
    {
        "file": "mm-conven",
        "title": "Conventional",
        "original": "grom_dm3 / Conventional",
        "original_author": 'Robert "Grom" McLachlan',
        "release_date": "4 November 1999",
        "players": "4-8",
        "gametypes": ["FFA", "2v2", "TDM", "Quad Hog"],
        "readme": None,
        "bsp": "grom_dm3.bsp",
        "source_urls": [("DondeQ2 map archive", "https://dondeq2.com/2017/10/24/webman-twists-map-collection/")],
        "summary": "A broad, readable arena with enough weapon spread and a Quad to turn public FFA into a real chase.",
        "history": "Grom's Conventional is one of those late-classic Quake II community maps that feels built for server rotation: clear rooms, strong routes, and a name that undersells how lively it gets once the Quad comes up.",
        "play": "Good for busy public play and light team games. The map has eight deathmatch starts, several major armors, and a Quad, so it benefits from players who will move rather than camp one room.",
    },
    {
        "file": "mm-crucible",
        "title": "The Crucible",
        "original": "ztn2dm5 / The Crucible",
        "original_author": 'Sten "ztn" Uusvali',
        "release_date": "6 September 1998",
        "players": "2-6",
        "gametypes": ["Duel", "FFA", "2v2"],
        "readme": "ztn2dm5-readme.txt",
        "bsp": "ztn2dm5.bsp",
        "source_urls": [("Quake II Netpack I: Extremities", "https://archive.org/details/QuakeIINetpackIExtremitiesUSAEurope")],
        "summary": "A ztn classic with layered control, sharp drops into danger, and enough weapon density to make every route feel contested.",
        "history": "The Crucible was part of Sten Uusvali's influential Quake II duel run and later appeared in Quake II Netpack I: Extremities. Its directness makes it easy to learn, but its timing game is still nasty.",
        "play": "Strong for duel and small FFA. The Body Armor, two Combat Armors, Mega Health, and Ammo Pack give players plenty to route, while the extra intermission cameras make it a polished spectator map.",
    },
    {
        "file": "mm-czero",
        "title": "Cold Zero",
        "original": "ven_dm2 / Cold Zero",
        "original_author": 'Kev "Ven" Pritchard',
        "release_date": "22 April 2001",
        "players": "3-8",
        "gametypes": ["FFA", "2v2", "TDM", "Instagib"],
        "readme": None,
        "bsp": "ven_dm2.bsp",
        "source_urls": [("PlanetQuake Level of the Week", "https://planetquake.gamespy.com/View38ff.html?id=177&view=LOTW.Detail")],
        "summary": "A lean contest-map layout with open reads, strong rail lines, and enough health to keep a public server from stalling.",
        "history": "Cold Zero came from Ven and is tied to PlanetQuake's Not Dead Yet! 600-brush contest coverage. That constraint gives the layout a pleasantly spare quality: readable, quick, and hard to hide in.",
        "play": "Best for FFA and light team play. It has nine deathmatch starts, a full weapon spread, and a Mega Health, so it can absorb more players than the pure duel maps.",
    },
    {
        "file": "mm-degen",
        "title": "Degeneration",
        "original": "paradm4 / Degeneration",
        "original_author": 'Jaan-Madis "paradies" Uusvali',
        "release_date": "14 June 1999",
        "players": "3-7",
        "gametypes": ["FFA", "2v2", "TDM"],
        "readme": None,
        "bsp": "paradm4.bsp",
        "source_urls": [("DondeQ2 map archive", "https://dondeq2.com/2017/10/24/webman-twists-map-collection/")],
        "summary": "A sturdy industrial fight map with two rockets, two chainguns, and enough armor shards to make chip damage matter.",
        "history": "Degeneration sits in the paradies run of Quake II maps and has the late-90s custom-map feel: practical geometry, quick weapon access, and a layout that rewards returning to fights before they cool off.",
        "play": "A good rotation pick when Aerowalk-sized maps are too cramped. It has no Mega Health, so armor timing and weapon pressure carry the match.",
    },
    {
        "file": "mm-fleshref",
        "title": "The Flesh Refinery",
        "original": "fleshref / q2rdm1",
        "original_author": 'Chris "Musashi" Walker',
        "release_date": "13 February 2001",
        "players": "2-4",
        "gametypes": ["Duel", "small FFA", "Power Screen experiment"],
        "readme": "fleshref-readme.txt",
        "bsp": "fleshref.bsp",
        "source_urls": [],
        "summary": "A compact tourney map that mixes longer fire corridors with close, sharp brawls and a rare Power Screen pickup.",
        "history": "Musashi described the original as a small tourney level built for a weathered arena under a red Stroggos sunset. Its Power Screen is unusual enough that the readme explains how to bind and use it.",
        "play": "Try it when the group wants a duel map with a slightly different powerup rhythm. The Power Screen is less oppressive than a Power Shield but still changes how frontal fights are taken.",
    },
    {
        "file": "mm-grind",
        "title": "Grind",
        "original": "grind / q2duel2",
        "original_author": 'Dennis "headshot" Kaltwasser',
        "release_date": "17 March 1998",
        "players": "2-8",
        "gametypes": ["Duel", "2v2", "FFA"],
        "readme": "grind-readme.txt",
        "bsp": "grind.bsp",
        "source_urls": [("DondeQ2 map archive", "https://dondeq2.com/2017/10/24/webman-twists-map-collection/")],
        "summary": "A crisp early Q2DM arena with a strong 2v2 recommendation and a secret Mega Health rhythm to spice up control.",
        "history": "headshot credited danimal's guns-and-ammo texture idea and recommended 2-on-2 play in the original readme. Grind is from the era when authors were still finding the Quake II duel language in public.",
        "play": "Works well as a bridge between duel and small-team play. Combat Armor appears three times in the register, and the Mega Health can swing a fight if players remember the route.",
    },
    {
        "file": "mm-ironox",
        "title": "Iron Oxide",
        "original": "ktdm1 / Iron Oxide",
        "original_author": 'Martin "Killer" Kilcoyne',
        "release_date": "18 September 1999",
        "players": "2-6",
        "gametypes": ["Duel", "small FFA", "2v2"],
        "readme": None,
        "bsp": "ktdm1.bsp",
        "source_urls": [("LvLWorld MKSTEEL readme", "https://lvlworld.com/readme/id%3A1250")],
        "summary": "A steel-and-rust arena with practical Quake II combat lines, generous bullets, and a compact armor game.",
        "history": "Iron Oxide is credited to Killer as KTDM1. Later author notes on LvLWorld list it as part of Killer's earlier Quake II work, giving the map a useful historical breadcrumb even where its original readme is missing.",
        "play": "A comfortable fit for small FFA or duel servers that want conventional Quake II weapons without a Mega Health centerpiece.",
    },
    {
        "file": "mm-kmach",
        "title": "The Killing Machine",
        "original": "ztn2dm2 / kmachine",
        "original_author": 'Sten "ztn" Uusvali',
        "release_date": "22 March 1998",
        "players": "2-8",
        "gametypes": ["FFA", "2v2", "casual Duel"],
        "readme": "ztn2dm2-readme.txt",
        "bsp": "ztn2dm2.bsp",
        "source_urls": [("Quake II Netpack I: Extremities", "https://archive.org/details/QuakeIINetpackIExtremitiesUSAEurope")],
        "summary": "A busy ztn arena with heavy weapons, multiple rails, a BFG, and a Quad for servers that want fireworks.",
        "history": "The Killing Machine is one of ztn's early Quake II deathmatch maps and part of the Netpack-era canon. It is less austere than the tight duel maps and better suited to noisy rotation play.",
        "play": "Ideal for casual FFA and 2v2. The BFG and Quad are not shy, so hosts wanting strict duel purity may prefer Painkiller or The Rage.",
    },
    {
        "file": "mm-llamp",
        "title": "Lava Lamp",
        "original": "4u2map12a / Lava Lamp",
        "original_author": "[4u2]Squirrel",
        "release_date": "2003 (exact day not located)",
        "players": "4-9",
        "gametypes": ["FFA", "TDM", "party server"],
        "readme": None,
        "bsp": "4u2map12a.bsp",
        "source_urls": [("quake2.com.ru listing", "https://quake2.com.ru/files/maps/5/")],
        "summary": "A big, energetic public-server map with rockets everywhere, nine starts, and enough lava-lit drama to justify the name.",
        "history": "The public listing for 4u2map12a identifies Lava Lamp as a Quake II map by [4u2]Squirrel. The local candidate readme did not match this map, so the guide preserves the BSP but treats the readme as not located.",
        "play": "Use it when the server population climbs. It has a BFG, two rocket launchers, lots of rockets and grenades, and a Mega Health, making it better for spectacle than for quiet duel discipline.",
    },
    {
        "file": "mm-longyd",
        "title": "The Longest Yard",
        "original": "q3dm17 / The Longest Yard",
        "original_author": "id Software",
        "release_date": "2 December 1999",
        "players": "4-16",
        "gametypes": ["FFA", "Instagib", "Clan Arena", "jump-pad chaos"],
        "readme": None,
        "bsp": None,
        "source_urls": [("Quake III Arena overview", "https://quake.fandom.com/wiki/Quake_III_Arena")],
        "summary": "A Quake III Arena landmark recast for Quake II: void space, long sightlines, jump-pad routes, and pure rail/rocket spectacle.",
        "history": "The Longest Yard is Quake III Arena's famous q3dm17, arriving with id Software's arena shooter in December 1999. This port makes the map a guest star in a Quake II ruleset.",
        "play": "Best for Instagib, casual FFA, and novelty Clan Arena. It has sixteen starts and very sparse weapon variety, so it is about aim, movement, and not falling into space.",
    },
    {
        "file": "mm-mcoil",
        "title": "Mortal Coil",
        "original": "broken2 / q2duel5",
        "original_author": 'Phil "Retinal" Chopp',
        "release_date": "24 April 1998",
        "players": "2-4",
        "gametypes": ["Duel", "small FFA", "2v2"],
        "readme": "broken2-readme.txt",
        "bsp": "broken2.bsp",
        "source_urls": [("DondeQ2 map archive", "https://dondeq2.com/2017/10/24/webman-twists-map-collection/")],
        "summary": "A lean duel arena with a Quad, clean weapon set, and the rough charm of early community experimentation.",
        "history": "Retinal called Mortal Coil a deathmatch dump, but the map's compact routing has kept it interesting. Its readme even notes that lighting was done without GLQuake, a nice little time capsule from 1998 mapping.",
        "play": "Good for one-on-one with occasional small FFA. The Quad makes it rowdier than some duel purists expect, so call it a pressure map rather than a sterile test chamber.",
    },
    {
        "file": "mm-negimp",
        "title": "Negative Impulse",
        "original": "vd6dm2 / Negative Impulse",
        "original_author": "-VooDoo6-",
        "release_date": "Not located",
        "players": "3-7",
        "gametypes": ["FFA", "2v2", "TDM"],
        "readme": None,
        "bsp": "vd6dm2.bsp",
        "source_urls": [("DondeQ2 map archive", "https://dondeq2.com/2017/10/24/webman-twists-map-collection/")],
        "summary": "A larger, moody map with a full weapon spread, Quad Damage, and enough health to support longer public fights.",
        "history": "Negative Impulse is preserved locally as vd6dm2 and appears in the DondeQ2 archive listing. A precise original release date and readme were not found in this pass.",
        "play": "A good FFA/TDM candidate. It has seven starts, two Super Shotguns, a Mega Health, Quad Damage, and strong armor coverage, so it rewards roaming control teams.",
    },
    {
        "file": "mm-oppress",
        "title": "The Oppressor",
        "original": "msdm5 / The Oppressor",
        "original_author": "Mike Shand",
        "release_date": "5 August 1998",
        "players": "4-10",
        "gametypes": ["FFA", "TDM", "2v2"],
        "readme": None,
        "bsp": "msdm5.bsp",
        "source_urls": [("Quake2.com Best Q2 Maps archive", "https://quake2.com/bestq2maps/aug26_sept23.htm")],
        "summary": "A ten-start arena with enough armor and health to hold a proper public brawl without collapsing into one spawn room.",
        "history": "The Oppressor appeared in late-90s Quake II map coverage as msdm5 by Mike Shand. Its rotation-friendly footprint makes the Muff Mode name feel especially apt.",
        "play": "Use it for FFA and light team games. It carries a broad weapon set, multiple jacket armors, and an Ammo Pack, so players can recover after messy fights.",
    },
    {
        "file": "mm-pkill",
        "title": "Painkiller",
        "original": "ztn2dm1 / painklr2",
        "original_author": 'Sten "ztn" Uusvali',
        "release_date": "22 January 1998",
        "players": "2-4",
        "gametypes": ["Duel", "small FFA", "Clan Arena"],
        "readme": "ztn2dm1-readme.txt",
        "bsp": "ztn2dm1.bsp",
        "source_urls": [("Quake II Netpack I: Extremities", "https://archive.org/details/QuakeIINetpackIExtremitiesUSAEurope")],
        "summary": "A tight ztn duel map with five starts, clean weapon access, and just enough item economy to punish lazy movement.",
        "history": "Painkiller is one of ztn's earliest Quake II duel maps and part of the same family that shaped a lot of competitive Q2 rotation thinking. Its readme names 2-4 players as the intended load.",
        "play": "Best as a duel map. The register includes Double Damage / Haste, so hosts should decide whether that belongs in their ruleset before putting it into a serious pool.",
    },
    {
        "file": "mm-rage",
        "title": "The Rage",
        "original": "ztn2dm3 / q2duel8",
        "original_author": 'Sten "ztn" Uusvali',
        "release_date": "10 April 1998",
        "players": "2-8",
        "gametypes": ["Duel", "FFA", "2v2"],
        "readme": "ztn2dm3-readme.txt",
        "bsp": "ztn2dm3.bsp",
        "source_urls": [("Quake II Netpack I: Extremities", "https://archive.org/details/QuakeIINetpackIExtremitiesUSAEurope")],
        "summary": "A classic ztn map with strong duel bones and enough spawns to flex into a lively small FFA.",
        "history": "The Rage followed Painkiller and The Killing Machine in ztn's 1998 run. The Muff Mode source map was copied from the final RC source as the final `mm-rage` reference.",
        "play": "Excellent for duel and small FFA. With no Quad but plenty of armor and Mega Health tension, it keeps control readable without being quiet.",
    },
    {
        "file": "mm-rail101",
        "title": "Railgun 101",
        "original": "2box4 / Railgun 101",
        "original_author": 'Thai "SwanSong" Pham',
        "release_date": "23 May 1998",
        "players": "2-6",
        "gametypes": ["Instagib", "rail practice", "aim warmups"],
        "readme": "2box4-readme.txt",
        "bsp": "2box4.bsp",
        "source_urls": [("DondeQ2 map archive", "https://dondeq2.com/2017/10/24/webman-twists-map-collection/")],
        "summary": "A stripped-down rail practice space: boxes, slugs, sightlines, and almost nothing to distract from aim.",
        "history": "SwanSong's original readme describes Railgun 101 as a small map for practicing railgun skills. It became a familiar warmup and Instagib-style room in Quake II server culture.",
        "play": "Use it for Instagib or warmups. The entity register intentionally has no weapon pickups, only slugs and a single armor shard, so the ruleset or server setup should supply the intended weapon behavior.",
    },
    {
        "file": "mm-reclam",
        "title": "Reclamation",
        "original": "kamq2dm3 / q2rdm7",
        "original_author": 'Corey "Kamarov" Peters',
        "release_date": "4 March 1999",
        "players": "2-4",
        "gametypes": ["Duel", "small FFA"],
        "readme": None,
        "bsp": "kamq2dm3.bsp",
        "source_urls": [("DondeQ2 map archive", "https://dondeq2.com/2017/10/24/webman-twists-map-collection/")],
        "summary": "A compact Kamarov map with a disciplined weapon set, modest health, and no oversized powerup safety net.",
        "history": "Reclamation is preserved as kamq2dm3 and is also associated with q2rdm7. The nearby kamq2dm4 readme mentions it as Kamarov's earlier Reclamation, but that readme belongs to Ubiquity and is not treated as this map's original readme.",
        "play": "A strong small-session map when the server wants quick fights without Quad or Mega Health. Four deathmatch starts keep it intimate.",
    },
    {
        "file": "mm-thunders",
        "title": "Thunderstruck",
        "original": "ra3map1 / Thunderstruck",
        "original_author": 'Till "Firestarter" Merker',
        "release_date": "24 July 2000",
        "players": "2-5",
        "gametypes": ["Duel", "Clan Arena", "Instagib", "rail/rocket practice"],
        "readme": None,
        "bsp": None,
        "source_urls": [("Rocket Arena 3 interview archive", "https://dondeq2.com/2018/10/03/kablooie-rocket-arena-3-interviews-with-senn-and-g1zm0/")],
        "summary": "A bare-knuckle arena classic: rail, rocket, Mega Health, and enough height change to make every miss feel expensive.",
        "history": "Thunderstruck began as a Rocket Arena 3 arena by Firestarter and later became one of the most recognizable small-arena layouts in Quake Live culture. The Muff Mode port brings that aim-and-positioning test into Quake II.",
        "play": "Excellent for Clan Arena, Instagib, and duel-flavored warmups. The item set is intentionally sparse, so it plays as a mechanics test more than a full economy map.",
    },
    {
        "file": "mm-undom",
        "title": "Unknown Domain",
        "original": "trdm04a / Unknown Domain",
        "original_author": 'Daniel "Trebz" Nolan',
        "release_date": "Not located",
        "players": "4-12",
        "gametypes": ["FFA", "TDM", "large public play"],
        "readme": None,
        "bsp": "trdm04a.bsp",
        "source_urls": [
            ("Daniel Nolan Quake 2 maps", "https://dnolan.com/quake-2/"),
            ("DondeQ2 map archive", "https://dondeq2.com/2017/10/24/webman-twists-map-collection/"),
        ],
        "summary": "A broad twelve-start map with a full arsenal, Quad Damage, Power Shield, and enough pickups to support a crowded server.",
        "history": "Daniel Nolan lists Unknown Domain as trdm04a in his Quake 2 map series. A matching original readme or exact release date was not located, but the author lineage is clear.",
        "play": "Use it when the population is high. It is one of the heavier maps in the final set, with BFG, Quad, Power Shield, Mega Health, and lots of small health.",
    },
    {
        "file": "mm-wicked",
        "title": "Wicked",
        "original": "cpm1a / Wicked",
        "original_author": "FxR|jude and Decker",
        "release_date": "3 January 2000; rereleased 23 October 2000",
        "players": "2-8",
        "gametypes": ["Duel", "Clan Arena", "small FFA"],
        "readme": None,
        "bsp": None,
        "source_urls": [
            ("LvLWorld Wicked readme", "https://lvlworld.com/readme/id%3A701"),
            ("ESReality Wicked coverage", "https://www.esreality.com/?a=coverage&event=2091304"),
        ],
        "summary": "A CPMA classic translated into Quake II language, with two rocket launchers, a Plasma Beam, and crisp route pressure.",
        "history": "Wicked is best known as cpm1a, a Quake III/Challenge ProMode Arena duel staple by FxR|jude and Decker. Its movement heritage is different from Quake II, which makes this port one of the guide's livelier cross-game guests.",
        "play": "Good for duel and CA-minded sessions. It has no Mega Health or Quad in this register, so fights are about armor, position, and weapon conversion.",
    },
    {
        "file": "mm-winpain",
        "title": "Window Pain",
        "original": "ra3map2c / Window Pain",
        "original_author": 'Adam "Senn" Bellefeuil',
        "release_date": "24 July 2000",
        "players": "2-12",
        "gametypes": ["Clan Arena", "Instagib", "FFA warmups"],
        "readme": None,
        "bsp": None,
        "source_urls": [
            ("Rocket Arena 3 Standard Maps listing", "https://steamcommunity.com/sharedfiles/filedetails/?id=1804631841"),
            ("Rocket Arena 3 interview archive", "https://dondeq2.com/2018/10/03/kablooie-rocket-arena-3-interviews-with-senn-and-g1zm0/"),
        ],
        "summary": "A Rocket Arena import with vertical exchanges, Plasma Beam pressure, and a simple item set that keeps the pace fast.",
        "history": "Window Pain is associated with Senn's ra3map2 set, a Rocket Arena 3 map pack remembered for focused arena combat. The Muff Mode version keeps that practice-arena spirit.",
        "play": "Best for CA, Instagib, or quick FFA warmups. Twelve starts make it roomy enough for a crowd, but the weapon set is narrow and aim-heavy.",
    },
]

WAYBACK_HD_MAIN = "https://web.archive.org/web/20031227060529/http://hd.ausgamers.com:80/main.html"
WAYBACK_HD_MAPCFG = "https://web.archive.org/web/20040104011615if_/http://hd.ausgamers.com:80/mapcfg.txt"
IA_NETPACK = "https://archive.org/details/QuakeIINetpackIExtremitiesUSAEurope"
IA_QUAKEUNITY = "https://archive.org/details/quakeunity.com"
IA_Q3_ARENA = "https://archive.org/details/quake-3-arena"
IA_EDL_KMACH = "https://archive.org/details/Purri_vs_Syanid_EDL8_Quake2_Final_map2"
IA_EDL_MCOIL = "https://archive.org/details/Purri_vs_Syanid_EDL8_Quake2_Final_map3"
IA_EDL_RAGE = "https://archive.org/details/Purri_vs_Damiah_map4_EDL7_Quake2"
PLANETQUAKE_AEROWALK = "https://planetquake.gamespy.com/View7878.html?id=194&view=LOTW.Detail"
PLANETQUAKE_CZERO = "https://planetquake.gamespy.com/View38ff.html?id=177&view=LOTW.Detail"
Q2SCENE_FILES = "https://q2scene.com/ds/index.php?op=files"
Q2SCENE_NADL = "https://q2scene.com/nadl/"
QDEVELS_DUEL = "https://dondeq2.com/2017/12/13/qdevels-proudly-presents-duel-mod-for-quake-2/"
WEBMAN_TWISTS = "https://dondeq2.com/2017/10/24/webman-twists-map-collection/"
RA3_INTERVIEW = "https://dondeq2.com/2018/10/03/kablooie-rocket-arena-3-interviews-with-senn-and-g1zm0/"
RA3_STEAM = "https://steamcommunity.com/sharedfiles/filedetails/?id=1804631841"
DNOLAN_Q2 = "https://dnolan.com/quake-2/"
QUAKE2_COM_RU_MAPS_2 = "https://quake2.com.ru/files/maps/2/"
QUAKE2_COM_RU_MAPS_5 = "https://quake2.com.ru/files/maps/5/"
Q3ARENA_NEWS = "https://www.q3arena.com/backend.php"
TASTY_MAPS = "https://tastyspleen.net/~quake2/baseq2/maps/"

EXTRA_SOURCE_URLS = {
    "mm-aerow": [
        ("PlanetQuake Aerowalk Level of the Week", PLANETQUAKE_AEROWALK),
        ("Wayback How Delightful map pack", WAYBACK_HD_MAIN),
        ("Wayback How Delightful mapcfg", WAYBACK_HD_MAPCFG),
        ("Q2Scene DemoSquad files", Q2SCENE_FILES),
        ("Internet Archive QuakeUnity site rip", IA_QUAKEUNITY),
    ],
    "mm-biorust": [
        ("Q2Scene DemoSquad files", Q2SCENE_FILES),
        ("North America Duel League", Q2SCENE_NADL),
    ],
    "mm-conven": [
        ("Wayback How Delightful map pack", WAYBACK_HD_MAIN),
        ("Wayback How Delightful mapcfg", WAYBACK_HD_MAPCFG),
        ("WEBMAN and twists collection", WEBMAN_TWISTS),
    ],
    "mm-crucible": [
        ("Wayback How Delightful map pack", WAYBACK_HD_MAIN),
        ("Wayback How Delightful mapcfg", WAYBACK_HD_MAPCFG),
        ("Q2Scene DemoSquad files", Q2SCENE_FILES),
    ],
    "mm-czero": [
        ("Wayback How Delightful mapcfg", WAYBACK_HD_MAPCFG),
        ("WEBMAN and twists collection", WEBMAN_TWISTS),
    ],
    "mm-degen": [("WEBMAN and twists collection", WEBMAN_TWISTS)],
    "mm-fleshref": [
        ("Wayback How Delightful map pack", WAYBACK_HD_MAIN),
        ("Wayback How Delightful mapcfg", WAYBACK_HD_MAPCFG),
        ("Q2Scene DemoSquad files", Q2SCENE_FILES),
    ],
    "mm-grind": [
        ("Wayback How Delightful map pack", WAYBACK_HD_MAIN),
        ("Wayback How Delightful mapcfg", WAYBACK_HD_MAPCFG),
    ],
    "mm-ironox": [("WEBMAN and twists collection", WEBMAN_TWISTS)],
    "mm-kmach": [
        ("Wayback How Delightful map pack", WAYBACK_HD_MAIN),
        ("Wayback How Delightful mapcfg", WAYBACK_HD_MAPCFG),
        ("Q2Scene DemoSquad files", Q2SCENE_FILES),
        ("Internet Archive EDL#8 final map 2", IA_EDL_KMACH),
    ],
    "mm-llamp": [
        ("Q3Arena news feed", Q3ARENA_NEWS),
        ("TastySpleen baseq2 map mirror", TASTY_MAPS),
    ],
    "mm-longyd": [
        ("Internet Archive Quake III Arena", IA_Q3_ARENA),
        ("Internet Archive QuakeUnity site rip", IA_QUAKEUNITY),
    ],
    "mm-mcoil": [
        ("Wayback How Delightful map pack", WAYBACK_HD_MAIN),
        ("Wayback How Delightful mapcfg", WAYBACK_HD_MAPCFG),
        ("QDeveLS Duel mod archive", QDEVELS_DUEL),
        ("Internet Archive EDL#8 final map 3", IA_EDL_MCOIL),
    ],
    "mm-negimp": [("WEBMAN and twists collection", WEBMAN_TWISTS)],
    "mm-oppress": [],
    "mm-pkill": [
        ("Wayback How Delightful map pack", WAYBACK_HD_MAIN),
        ("Wayback How Delightful mapcfg", WAYBACK_HD_MAPCFG),
        ("Q2Scene DemoSquad files", Q2SCENE_FILES),
    ],
    "mm-rage": [
        ("Wayback How Delightful map pack", WAYBACK_HD_MAIN),
        ("Wayback How Delightful mapcfg", WAYBACK_HD_MAPCFG),
        ("QDeveLS Duel mod archive", QDEVELS_DUEL),
        ("Q2Scene DemoSquad files", Q2SCENE_FILES),
        ("Internet Archive EDL#7 map 4", IA_EDL_RAGE),
    ],
    "mm-rail101": [
        ("Q2Scene DemoSquad files", Q2SCENE_FILES),
        ("WEBMAN and twists collection", WEBMAN_TWISTS),
    ],
    "mm-reclam": [
        ("Wayback How Delightful mapcfg", WAYBACK_HD_MAPCFG),
        ("Q2Scene DemoSquad files", Q2SCENE_FILES),
        ("WEBMAN and twists collection", WEBMAN_TWISTS),
    ],
    "mm-thunders": [
        ("Internet Archive QuakeUnity site rip", IA_QUAKEUNITY),
        ("Rocket Arena 3 Standard Maps listing", RA3_STEAM),
    ],
    "mm-undom": [
        ("WEBMAN and twists collection", WEBMAN_TWISTS),
        ("Daniel Nolan Quake 2 maps", DNOLAN_Q2),
    ],
    "mm-wicked": [
        ("Wayback How Delightful map pack", WAYBACK_HD_MAIN),
        ("Wayback How Delightful mapcfg", WAYBACK_HD_MAPCFG),
        ("Internet Archive QuakeUnity site rip", IA_QUAKEUNITY),
    ],
    "mm-winpain": [
        ("Internet Archive QuakeUnity site rip", IA_QUAKEUNITY),
        ("Rocket Arena 3 Standard Maps listing", RA3_STEAM),
        ("Rocket Arena 3 interview archive", RA3_INTERVIEW),
    ],
}

ARCHIVE_NOTES = {
    "mm-aerow": [
        "PlanetQuake's Level of the Week coverage names Mattias \"Preacher\" Konradsson, notes that AeroWalk first appeared in Quake, and describes the Quake II version as a conversion built around a four-level central atrium.",
        "The 2003 Wayback capture of How Delightful puts `aeroq2` in an OSP-ready custom-map pack and its `mapcfg.txt` lists it for 1v1, FFA, and TDM rotation under Preacher's name.",
        "Archive.org's QuakeUnity site rip preserves later `q3a-map-aeroq2.zip` traces, a small but telling sign of how Aerowalk kept moving across engines after the Quake II port.",
    ],
    "mm-biorust": [
        "Bio Rust is a later duel-map survivor rather than a 1998 fossil. Q2Scene's DemoSquad files list `koldduel1.zip`, and the NADL page places `koldduel1` beside q2dm1, q2dm3, q2duel1, q2duel5, and the ztn maps.",
        "The author trail is thin in classic Wayback captures, but the map's continued league and demo presence is unusually strong for a 2008 community duel release.",
    ],
    "mm-conven": [
        "The How Delightful Wayback capture lists `grom_dm3` as Conventional by Grom, with the companion mapcfg putting it in the 1v1/FFA/TDM custom pool.",
        "The WEBMAN/twists collection also links `grom_dm3.bsp` through Wayback. The Muff Mode source map expands the author credit to Robert \"Grom\" McLachlan.",
    ],
    "mm-crucible": [
        "ztn's preserved readme gives the old contact trail and homepage, while Quake II Netpack I: Extremities anchors The Crucible in the 1998 commercial add-on era of selected community maps.",
        "Both How Delightful and Q2Scene list `ztn2dm5`, showing the map kept circulating as a practical server and demo dependency after the Netpack moment.",
    ],
    "mm-czero": [
        "PlanetQuake's Level of the Week page says Kev \"Ven\" Pritchard's Cold Zero won the Not Dead Yet! 600-brush Quake II contest, which explains the map's clean, constraint-driven read.",
        "How Delightful's mapcfg lists `q2rdm9` as Cold Zero by Ven and notes the underlying `ven_dm2` identity. WEBMAN/twists separately lists `ven_dm2.bsp` as Cold Zero by Ven.",
    ],
    "mm-degen": [
        "The WEBMAN/twists archive listing is the main historic trace located in this pass: `paradm4.bsp` appears as Degeneration by paradies.",
        "The Muff Mode source map expands the author credit to Jaan-Madis \"paradies\" Uusvali. No direct Archive.org item or original readme was found, so the page treats the date as coming from local/source lineage.",
    ],
    "mm-fleshref": [
        "Musashi's readme names an Angelfire home page, describes The Flesh Refinery as a small tourney level, and calls out the Power Screen, which is rare enough in duel-style Q2 to deserve a warning.",
        "How Delightful carries both `fleshref` and `q2rdm1` as Flesh Refinery by Musashi, while Q2Scene keeps `q2rmappack.zip` listed as the q2rdm1-q2rdm15 map pack.",
    ],
    "mm-grind": [
        "headshot's readme credits Dennis Kaltwasser, gives the early 1998 release date, and recommends the map for 2-on-2 as well as larger deathmatch.",
        "How Delightful lists `grind` by headshot in the same custom pack as `ptrip`, giving this author two visible slots in the archived Australian OSP rotation.",
    ],
    "mm-ironox": [
        "The WEBMAN/twists archive listing identifies `ktdm1.bsp` as Iron Oxide by Killer, which is the strongest located archive trace for the original Q2 map.",
        "The later LvLWorld MKSTEEL readme ties Martin \"Killer\" Kilcoyne's Quake II work to his later Quake III mapping, giving the author trail a second breadcrumb.",
    ],
    "mm-kmach": [
        "The Killing Machine is one of the best-documented ztn maps here: Netpack gives it 1998 commercial-pack context, How Delightful lists both `kmachine` and `ztn2dm2`, and Q2Scene still offers `ztn2dm2.zip` as a map dependency.",
        "Archive.org also has the EDL#8 final map 2 item, whose metadata identifies the played map as The Killing Machine / `ztn2dm2`, a neat proof of its later duel life.",
    ],
    "mm-llamp": [
        "Lava Lamp did not turn up as a first-class Archive.org item in this pass. The strongest located public traces are quake2.com.ru's map listing and long-running mirrors such as TastySpleen's baseq2 map index.",
        "A Q3Arena news-feed snippet says [4u2]Squirrel's Lava Lamp placed second behind Maric's Pile O' Rocks, which helps explain why this public-server piece had enough visibility to survive.",
    ],
    "mm-longyd": [
        "The original is id Software's Q3DM17 from Quake III Arena; the Internet Archive has Quake III Arena software items that document the 1999 source game context.",
        "Archive.org's QuakeUnity site rip includes `q3dm17vortex.zip`, showing the Longest Yard idea kept producing derivative archive material long after the stock Quake III release.",
    ],
    "mm-mcoil": [
        "Retinal's readme is wonderfully blunt, calling Mortal Coil a deathmatch dump, and it also lists earlier Quake maps by the author.",
        "How Delightful lists `broken2` as Mortal Coil by Retinal, QDeveLS' Duel mod archive gives the map its `q2duel5` identity, and Archive.org's EDL#8 final map 3 metadata identifies it as Mortal Coil / `q2duel5`.",
    ],
    "mm-negimp": [
        "The WEBMAN/twists collection lists `vd6dm2.bsp` as Negative Impulse by VooDoo6, which is the cleanest located historic trace for the name and author.",
        "No original readme or direct Archive.org item was found; the preserved BSP and Muff Mode source map therefore carry most of the local certainty.",
    ],
    "mm-oppress": [
        "Quake2.com's old Best Q2 Maps coverage names The Oppressor as `msdm5.zip`, credits Mike Shand, and supplies the August 1998 date used here.",
        "A direct Archive.org/Wayback item for the original zip was not located in this pass, so this page treats the public review/archive page as the strongest external evidence.",
    ],
    "mm-pkill": [
        "Painkiller is anchored by ztn's original readme, Netpack's 1998 community-map selection, How Delightful's `ztn2dm1` entry, and Q2Scene's `ztn2dm1.zip` file listing.",
        "The How Delightful page also lists `painklr2`, a nearby Painkiller II variant, which is useful context for the map-name confusion around Painkiller/Painkiller II in old Q2 archives.",
    ],
    "mm-rage": [
        "The Rage carries ztn's preserved readme trail and appears in How Delightful as `ztn2dm3`; QDeveLS' Duel archive also calls it `q2duel8` by Sten \"ztn\" Uusvali.",
        "Archive.org's EDL#7 map 4 item identifies the played map as `ztn2dm3`, \"The Rage\", showing the map was still a stage for top-level duel years after release.",
    ],
    "mm-rail101": [
        "SwanSong's readme describes Railgun 101 as a small map for practicing railgun skills, and that practical purpose matches its later archive life.",
        "Q2Scene lists `2box4.bsp` as a map file, while WEBMAN/twists lists `2box4.bsp` as Railgun 101 by SwanSong.",
    ],
    "mm-reclam": [
        "How Delightful's mapcfg lists `q2rdm7` as Reclamation by Kamarov and notes the underlying `kamq2dm3` identity.",
        "Q2Scene keeps `q2rmappack.zip` listed as the q2rdm1-q2rdm15 map pack, while WEBMAN/twists gives another archived route back to `kamq2dm3.bsp`.",
    ],
    "mm-thunders": [
        "The Rocket Arena 3 Standard Maps listing identifies `ra3map1b` as Thunderstruck and credits Firestarter as the original author.",
        "Archive.org's QuakeUnity site rip preserves RA3 package traces such as `pur3ra3map1.zip` and `OspRa3map1.zip`, showing this arena's life inside the broader RA3/OSP file ecosystem.",
    ],
    "mm-undom": [
        "Daniel Nolan's own Quake 2 page lists Unknown Domain as `trdm04a`, and WEBMAN/twists lists `trdm04a.bsp` as Unknown Domain by Trebz.",
        "No exact original release date or readme was located, but the author and filename trail is unusually direct for a map without its text file.",
    ],
    "mm-wicked": [
        "LvLWorld's cpm1a readme says Wicked was by FxR|jude and Decker, released in January 2000 and re-released in October, and notes the design began as a Quake II FFA map before becoming a Quake III duel staple.",
        "How Delightful's 2003 mapcfg lists `foodm8` as Wicked by jude, while Archive.org's QuakeUnity rip preserves later Wicked-related media traces.",
    ],
    "mm-winpain": [
        "The RA3 Standard Maps listing identifies `ra3map2c` as Window Pain and credits Senn as original author.",
        "The Kablooie/Donde archive interview identifies ra3map2 as Senn's Liquid Blue pack and calls Window Pain one of its 1v1 arenas. QuakeUnity also preserves RA3 map2 package traces.",
    ],
}


def parse_entities_from_bsp(path: Path) -> list[dict[str, str]]:
    data = path.read_bytes()
    if data[:4] != b"IBSP":
        raise ValueError(f"Not a BSP: {path}")
    offset, length = struct.unpack_from("<ii", data, 8)
    text = data[offset : offset + length].split(b"\0", 1)[0].decode("latin1", "replace")
    entities = []
    for block in re.findall(r"\{([^{}]*)\}", text, re.S):
        entities.append(dict(re.findall(r'"([^"]*)"\s+"([^"]*)"', block)))
    return entities


def parse_first_entity_map(path: Path) -> dict[str, str]:
    text = path.read_text(encoding="latin1", errors="replace")
    match = re.search(r"\{(.*?)\}", text, re.S)
    if not match:
        return {}
    return dict(re.findall(r'"([^"]*)"\s+"([^"]*)"', match.group(1)))


def item_rows(final_maps: Path, stem: str) -> list[tuple[str, str]]:
    counts = Counter(entity.get("classname", "") for entity in parse_entities_from_bsp(final_maps / f"{stem}.bsp"))
    rows = []
    for labels, category in [
        (SPAWN_NAMES, "Spawns"),
        (WEAPON_NAMES, "Weapons"),
        (AMMO_NAMES, "Ammo"),
        (UTILITY_NAMES, "Utility"),
    ]:
        entries = [(label, counts[classname]) for classname, label in labels.items() if counts[classname] > 0]
        if entries:
            rows.append((category, ", ".join(f"{label} x{count}" for label, count in entries)))
        elif category == "Weapons":
            rows.append((category, "No weapon pickups"))
    return rows


def status_for(stem: str, final: bool = False) -> str:
    if final:
        return "Final release"
    match = re.search(r"-rc(\d+)$", stem)
    if match:
        return f"Release candidate {match.group(1)}"
    match = re.search(r"-b(\d+)$", stem)
    if match:
        return f"Beta {match.group(1)}"
    match = re.search(r"-a(\d+)$", stem)
    if match:
        return f"Alpha {match.group(1)}"
    return "In development"


def status_rank(stem: str) -> tuple[int, int]:
    for pattern, rank in [(r"-rc(\d+)$", 3), (r"-b(\d+)$", 2), (r"-a(\d+)$", 1)]:
        match = re.search(pattern, stem)
        if match:
            return rank, int(match.group(1) or 0)
    return 0, 0


def clean_title(title: str) -> str | None:
    title = title.strip()
    title = re.sub(r"\s*\[(?:ALPHA|Alpha|BETA|Beta|RC)[^\]]*\]\s*$", "", title)
    return title.replace("  ", " ").strip() or None


def title_key(title: str | None) -> str:
    return re.sub(r"[^a-z0-9]+", "", (title or "").lower())


def markdown_link(label: str, target: str) -> str:
    return f"[{label}]({target})"


def cell(value: str) -> str:
    return str(value).replace("|", "\\|")


def file_link_or_note(folder: str, name: str | None, note: str = "Not located") -> str:
    return markdown_link(name, f"{folder}/{name}") if name else note


def item_table(rows: list[tuple[str, str]]) -> str:
    out = ["| Category | Register |", "| --- | --- |"]
    out.extend(f"| {category} | {register} |" for category, register in rows)
    return "\n".join(out)


def combined_source_urls(map_info: dict[str, object]) -> list[tuple[str, str]]:
    seen: set[str] = set()
    rows: list[tuple[str, str]] = []
    for label, url in list(map_info.get("source_urls", [])) + EXTRA_SOURCE_URLS.get(str(map_info["file"]), []):
        if url in seen:
            continue
        seen.add(url)
        rows.append((label, url))
    return rows


def archive_note_list(stem: str) -> str:
    notes = ARCHIVE_NOTES.get(stem, [])
    if not notes:
        return "No extra archive or author notes were located during this pass."
    return "\n".join(f"- {note}" for note in notes)


def sources_list(map_info: dict[str, object]) -> str:
    stem = map_info["file"]
    lines = [f"- Final Muff Mode BSP/source data: [source-maps/{stem}.map](source-maps/{stem}.map)."]
    if map_info.get("readme"):
        lines.append(f"- Preserved original readme: [original-readmes/{map_info['readme']}](original-readmes/{map_info['readme']}).")
    if map_info.get("bsp"):
        lines.append(f"- Preserved original BSP: [original-bsps/{map_info['bsp']}](original-bsps/{map_info['bsp']}).")
    for label, url in combined_source_urls(map_info):
        lines.append(f"- {label}: [{url}]({url}).")
    return "\n".join(lines)


def select_dev_entries(dev_src: Path, dev_bsps: Path) -> list[dict[str, object]]:
    final_title_keys = {title_key(info["title"]) for info in FINAL_MAPS}
    excluded_title_keys = {title_key(title) for title in ["The Pits", "Hammertime!", "Hammertime", "The Camping Grounds"]}
    excluded_stems = {"mm-pits-rc1", "mm-hammer-rc1", "mm-campgr-rc1"}
    stem_records: dict[str, dict[str, object]] = {}

    for path in list(dev_src.glob("mm-*.map")) + list(dev_bsps.glob("mm-*.bsp")):
        stem = path.stem
        if stem in excluded_stems or re.search(r"-build\d*$", stem):
            continue
        record = stem_records.setdefault(stem, {"stem": stem, "source": None, "bsp": None, "title": None, "mtime": 0.0})
        record["mtime"] = max(float(record["mtime"]), path.stat().st_mtime)
        if path.suffix.lower() == ".map":
            record["source"] = path
            title = clean_title(parse_first_entity_map(path).get("message", ""))
        else:
            record["bsp"] = path
            try:
                entities = parse_entities_from_bsp(path)
                title = clean_title(entities[0].get("message", "") if entities else "")
            except Exception:
                title = None
        if title:
            record["title"] = title

    grouped: dict[str, list[dict[str, object]]] = defaultdict(list)
    for record in stem_records.values():
        if not record["bsp"] or not record["source"]:
            continue
        title = record["title"] or re.sub(r"-(rc|b|a)\d*$", "", str(record["stem"])).replace("mm-", "").replace("-", " ").title()
        record["title"] = title
        key = title_key(str(title))
        if key in final_title_keys or key in excluded_title_keys:
            continue
        grouped[key].append(record)

    selected = []
    for records in grouped.values():
        def sort_key(record: dict[str, object]) -> tuple[int, int, int, int, float]:
            rank, number = status_rank(str(record["stem"]))
            has_source = 1 if record["source"] else 0
            has_bsp = 1 if record["bsp"] else 0
            return rank, number, has_source, has_bsp, float(record["mtime"])

        selected.append(sorted(records, key=sort_key, reverse=True)[0])
    return sorted(selected, key=lambda record: str(record["title"]).lower())


def copy_assets(args: argparse.Namespace, docs: Path) -> tuple[list[tuple[str, str, str]], list[dict[str, object]]]:
    source_dir = docs / "source-maps"
    readme_dir = docs / "original-readmes"
    original_bsp_dir = docs / "original-bsps"
    dev_source_dir = docs / "dev-source-maps"

    for directory in [docs, source_dir, readme_dir, original_bsp_dir, dev_source_dir]:
        directory.mkdir(parents=True, exist_ok=True)

    for page in docs.glob("mm-*.md"):
        page.unlink()
    for directory in [source_dir, readme_dir, original_bsp_dir, dev_source_dir]:
        for path in directory.iterdir():
            if path.is_file():
                path.unlink()

    source_rows = []
    for info in FINAL_MAPS:
        source_name = f"{info['file']}.map"
        copied_from = "mm-rage-rc6.map" if info["file"] == "mm-rage" else source_name
        shutil.copy2(args.final_src / copied_from, source_dir / source_name)
        source_rows.append((str(info["file"]), source_name, copied_from))

    readme_sources = {
        "aerowalk-readme.txt": args.originals / "aerowalk" / "pack" / "docs" / "aerowalk-readme.txt",
        "2box4-readme.txt": args.originals / "2box4" / "pack" / "docs" / "2box4-readme.txt",
        "ztn2dm1-readme.txt": args.originals / "ztn2dm1" / "pack" / "docs" / "ztn2dm1-readme.txt",
        "ztn2dm2-readme.txt": args.originals / "ztn2dm2" / "pack" / "docs" / "ztn2dm2-readme.txt",
        "ztn2dm3-readme.txt": args.originals / "ztn2dm3" / "pack" / "docs" / "ztn2dm3-readme.txt",
        "ztn2dm5-readme.txt": args.originals / "ztn2dm5" / "pack" / "docs" / "ztn2dm5-readme.txt",
        "broken2-readme.txt": args.originals / "broken2" / "pack" / "docs" / "broken2-readme.txt",
        "fleshref-readme.txt": args.originals / "fleshref" / "pack" / "docs" / "fleshref-readme.txt",
        "grind-readme.txt": args.originals / "grind" / "pack" / "docs" / "grind-readme.txt",
    }
    for name, src in readme_sources.items():
        shutil.copy2(src, readme_dir / name)

    original_bsp_sources = {
        "aerowalk.bsp": args.originals / "aerowalk" / "pack" / "maps" / "aerowalk.bsp",
        "2box4.bsp": args.originals / "2box4" / "pack" / "maps" / "2box4.bsp",
        "ztn2dm1.bsp": args.originals / "ztn2dm1" / "pack" / "maps" / "ztn2dm1.bsp",
        "ztn2dm2.bsp": args.originals / "ztn2dm2" / "pack" / "maps" / "ztn2dm2.bsp",
        "ztn2dm3.bsp": args.originals / "ztn2dm3" / "pack" / "maps" / "ztn2dm3.bsp",
        "ztn2dm5.bsp": args.originals / "ztn2dm5" / "pack" / "maps" / "ztn2dm5.bsp",
        "broken2.bsp": args.originals / "broken2" / "pack" / "maps" / "broken2.bsp",
        "fleshref.bsp": args.originals / "fleshref" / "pack" / "maps" / "fleshref.bsp",
        "grind.bsp": args.originals / "grind" / "pack" / "maps" / "grind.bsp",
        "grom_dm3.bsp": args.originals / "grom_dm3" / "pack" / "maps" / "grom_dm3.bsp",
        "4u2map12a.bsp": args.originals / "4u2map12a" / "pack" / "maps" / "4u2map12a.bsp",
        "vd6dm2.bsp": args.originals / "vd6dm2" / "pack" / "maps" / "vd6dm2.bsp",
        "koldduel1.bsp": args.steam_maps / "koldduel1.bsp",
        "ven_dm2.bsp": args.steam_maps / "ven_dm2.bsp",
        "paradm4.bsp": args.steam_maps / "paradm4.bsp",
        "ktdm1.bsp": args.steam_maps / "ktdm1.bsp",
        "msdm5.bsp": args.steam_maps / "msdm5.bsp",
        "kamq2dm3.bsp": args.steam_maps / "kamq2dm3.bsp",
        "trdm04a.bsp": args.steam_maps / "trdm04a.bsp",
    }
    for name, src in original_bsp_sources.items():
        shutil.copy2(src, original_bsp_dir / name)

    dev_selected = select_dev_entries(args.dev_src, args.dev_bsps)
    for record in dev_selected:
        if record["source"]:
            shutil.copy2(record["source"], dev_source_dir / f"{record['stem']}.map")

    return source_rows, dev_selected


def write_map_pages(args: argparse.Namespace, docs: Path) -> None:
    for info in FINAL_MAPS:
        original_readme = file_link_or_note("original-readmes", info.get("readme"))
        original_bsp = file_link_or_note("original-bsps", info.get("bsp"), note="Not preserved locally")
        source_map = markdown_link(f"{info['file']}.map", f"source-maps/{info['file']}.map")
        gametypes = ", ".join(info["gametypes"])
        content = f"""# {info['title']}

[Map Guide](index.md) | [Gameplay Reference](../gameplay-reference.md)

| Field | Details |
| --- | --- |
| Filename | `{info['file']}` |
| Development status | Final release |
| Original map | {cell(info['original'])} |
| Original author | {cell(info['original_author'])} |
| Original release date | {cell(info['release_date'])} |
| Recommended players | {cell(info['players'])} |
| Good fits | {cell(gametypes)} |
| Original readme | {original_readme} |
| Original BSP | {original_bsp} |
| Remaster source map | {source_map} |

## Why Play It

{info['summary']}

## Where It Came From

{info['history']}

## Archive And Author Notes

{archive_note_list(str(info['file']))}

## How It Plays

{info['play']}

## Item Register

Counts are taken from the final Muff Mode BSP entity data.

{item_table(item_rows(args.final_maps, str(info['file'])))}

## Preserved Files

| File type | Link |
| --- | --- |
| Remaster source map | {source_map} |
| Original readme | {original_readme} |
| Original BSP | {original_bsp} |

## Sources

{sources_list(info)}
"""
        (docs / f"{info['file']}.md").write_text(content, encoding="utf-8", newline="\n")


def write_index(docs: Path, dev_selected: list[dict[str, object]]) -> None:
    by_file = {str(info["file"]): info for info in FINAL_MAPS}
    quick_picks = [
        ("Duel pressure", ["mm-aerow", "mm-rage", "mm-pkill", "mm-mcoil", "mm-wicked"]),
        ("Public FFA or TDM", ["mm-conven", "mm-degen", "mm-llamp", "mm-undom", "mm-oppress"]),
        ("Rail and aim night", ["mm-rail101", "mm-thunders", "mm-winpain", "mm-longyd"]),
        ("ztn classics", ["mm-crucible", "mm-kmach", "mm-pkill", "mm-rage"]),
        ("Cross-game guests", ["mm-longyd", "mm-wicked", "mm-thunders", "mm-winpain"]),
    ]
    quick_lines = ["| Mood | Start with |", "| --- | --- |"]
    for mood, files in quick_picks:
        links = ", ".join(markdown_link(str(by_file[file]["title"]), f"{file}.md") for file in files)
        quick_lines.append(f"| {mood} | {links} |")

    final_lines = ["| Map | Filename | Original lineage | Original release date | Good fits |", "| --- | --- | --- | --- | --- |"]
    for info in FINAL_MAPS:
        stem = str(info["file"])
        title = str(info["title"])
        final_lines.append(
            f"| {markdown_link(title, f'{stem}.md')} | `{stem}` | {cell(info['original'])} | {cell(info['release_date'])} | {cell(', '.join(info['gametypes']))} |"
        )

    dev_lines = ["| Map | Filename | Status | Source map | BSP in dev folder |", "| --- | --- | --- | --- | --- |"]
    for record in dev_selected:
        stem = str(record["stem"])
        source_link = markdown_link(f"{stem}.map", f"dev-source-maps/{stem}.map") if record["source"] else "No selected source map located"
        dev_lines.append(f"| {cell(record['title'])} | `{stem}` | {status_for(stem)} | {source_link} | {'Yes' if record['bsp'] else 'No'} |")

    preserved_lines = ["| Map | Original readme | Original BSP | Remaster source map |", "| --- | --- | --- | --- |"]
    for info in FINAL_MAPS:
        stem = str(info["file"])
        title = str(info["title"])
        preserved_lines.append(
            f"| {markdown_link(title, f'{stem}.md')} | {file_link_or_note('original-readmes', info.get('readme'))} | {file_link_or_note('original-bsps', info.get('bsp'), note='Not preserved locally')} | {markdown_link(f'{stem}.map', f'source-maps/{stem}.map')} |"
        )

    content = f"""# Muff Mode Map Guide

[README](../../README.md) | [Gameplay Reference](../gameplay-reference.md) | [Level Design Guide](../level-design-guide.md)

Muff Mode's `mm-*` maps are a small museum with live rockets in it: Quake II duel standards, late-90s community curios, Rocket Arena and Quake III visitors, and a few stranger public-server pieces rebuilt for the rerelease era.

This guide covers the current authoritative final set from `mm-maps-finals-1`. Each detail page includes the Muff Mode filename, release status, original release date where it could be found, a short history, play notes, preserved files, and a level item register read from the final BSP. Work-in-progress coverage is limited to the active local dev tree and keeps only the newest apparent state for each map identity. The release package currently carries a curated subset plus several separately named ports; the packaged structured pool, rather than this archive catalog, is the exact installed-map manifest.

Packaged BSPs retain the short map-bundle IDs. In particular, use `mm-czero`, `mm-kmach`, `mm-rail101`, and `mm-reclam` in consoles and server configs; the catalog, structured pool, cycle, and source archive all keep those names aligned.

## Status Key

| Suffix | Meaning |
| --- | --- |
| No suffix | Final release |
| `-rcN` | Release candidate N |
| `-bN` | Beta N |
| `-aN` | Alpha N |

## Quick Picks

{chr(10).join(quick_lines)}

## Final Maps

{chr(10).join(final_lines)}

## In Development

These entries are copied only from `MuffMode-Map-Remasters/dev`. Older states for the same map identity are omitted. Internal work snapshots and entries without both a source `.map` and compiled `.bsp` are excluded.

{chr(10).join(dev_lines)}

## Preserved Files

Original BSPs are kept under [original-bsps](original-bsps/README.md), original readmes under [original-readmes](original-readmes/README.md), and remaster source `.map` files under [source-maps](source-maps/README.md). The source maps are deliberately separate from BSP files.

{chr(10).join(preserved_lines)}

## Research Notes

- Local final BSPs and source maps provided the included filenames, final status, worldspawn titles, and entity/item counts.
- Preserved local readmes supplied exact dates and author notes for Aerowalk, Railgun 101, the ztn maps, Mortal Coil, The Flesh Refinery, and Grind.
- The Steam Quake II rerelease installation supplied several original community BSPs where readmes were not present locally.
- Internet Archive and Wayback Machine research added several useful archive trails: [Quake II Netpack I: Extremities]({IA_NETPACK}), the 2003 [How Delightful custom-map pack]({WAYBACK_HD_MAIN}) and [mapcfg]({WAYBACK_HD_MAPCFG}), the [QuakeUnity site rip]({IA_QUAKEUNITY}), and Archive.org-hosted EDL finals videos linked from the individual ztn and Mortal Coil pages.
- DondeQ2's [WEBMAN and twists Quake 2 Map Collection]({WEBMAN_TWISTS}), Q2Scene's [DemoSquad files]({Q2SCENE_FILES}), PlanetQuake Level of the Week pages, LvLWorld readmes, and author pages filled in names where Archive.org had only filename-level evidence.
- A few maps still have deliberately modest history notes because no direct original readme or first-class Archive.org item was found for them. Those pages say so rather than pretending a weak mirror listing is a primary source.
"""
    (docs / "index.md").write_text(content, encoding="utf-8", newline="\n")


def write_asset_readmes(docs: Path, source_rows: list[tuple[str, str, str]], dev_selected: list[dict[str, object]]) -> None:
    source_dir = docs / "source-maps"
    readme_dir = docs / "original-readmes"
    original_bsp_dir = docs / "original-bsps"
    dev_source_dir = docs / "dev-source-maps"

    lines = [
        "# Final Remaster Source Maps",
        "",
        "These `.map` files are copied from the authoritative final map source set and are kept separate from BSP files for anyone who wants to inspect how the remasters and ports were built.",
        "",
        "| Final map | Source map | Copied from |",
        "| --- | --- | --- |",
    ]
    lines.extend(f"| `{stem}` | [{source_name}]({source_name}) | `{copied_from}` |" for stem, source_name, copied_from in source_rows)
    (source_dir / "README.md").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")

    lines = [
        "# Original Readmes",
        "",
        "These are original map readmes that matched the final map lineage during this pass. Candidate files that described a different map were left out rather than preserved under a misleading name.",
        "",
        "| Muff Mode map | Original map | Readme |",
        "| --- | --- | --- |",
    ]
    for info in FINAL_MAPS:
        readme = markdown_link(str(info["readme"]), str(info["readme"])) if info.get("readme") else "Not located"
        lines.append(f"| `{info['file']}` | {cell(info['original'])} | {readme} |")
    (readme_dir / "README.md").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")

    lines = [
        "# Original BSPs",
        "",
        "These historical BSPs are preserved for comparison and research. They are not the Muff Mode remaster BSPs and are intentionally kept outside the playable release map directory.",
        "",
        "| Muff Mode map | Original map | Original BSP | Original readme |",
        "| --- | --- | --- | --- |",
    ]
    for info in FINAL_MAPS:
        bsp = markdown_link(str(info["bsp"]), str(info["bsp"])) if info.get("bsp") else "Not preserved locally"
        readme = markdown_link(str(info["readme"]), f"../original-readmes/{info['readme']}") if info.get("readme") else "Not located"
        lines.append(f"| `{info['file']}` | {cell(info['original'])} | {bsp} | {readme} |")
    (original_bsp_dir / "README.md").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")

    lines = [
        "# Development Source Maps",
        "",
        "This folder mirrors only selected `.map` files from `MuffMode-Map-Remasters/dev`, one newest apparent state per map identity. It does not contain BSP files, and maps are included only when both the source `.map` and matching compiled `.bsp` exist in the dev tree. Internal work snapshots are excluded.",
        "",
        "| Map | Filename | Status | Source map | BSP in dev folder |",
        "| --- | --- | --- | --- | --- |",
    ]
    for record in dev_selected:
        stem = str(record["stem"])
        src = markdown_link(f"{stem}.map", f"{stem}.map") if record["source"] else "No selected source map located"
        lines.append(f"| {cell(record['title'])} | `{stem}` | {status_for(stem)} | {src} | {'Yes' if record['bsp'] else 'No'} |")
    (dev_source_dir / "README.md").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate Muff Mode map guide pages and preserved map assets.")
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--final-root", type=Path, default=Path(r"E:\Repositories\mm-maps-finals-1"))
    parser.add_argument("--originals", type=Path, default=Path(r"E:\Repositories\MuffMode-Map-Remasters\originals"))
    parser.add_argument("--dev-root", type=Path, default=Path(r"E:\Repositories\MuffMode-Map-Remasters\dev"))
    parser.add_argument("--steam-maps", type=Path, default=Path(r"C:\Program Files (x86)\Steam\steamapps\common\Quake 2\rerelease\baseq2\maps"))
    args = parser.parse_args()
    args.final_maps = args.final_root / "maps"
    args.final_src = args.final_root / "src"
    args.dev_src = args.dev_root / "src" / "maps"
    args.dev_bsps = args.dev_root / "maps"
    return args


def main() -> None:
    args = parse_args()
    docs = args.repo / "docs" / "maps"
    for path in [args.final_maps, args.final_src, args.originals, args.dev_src, args.dev_bsps, args.steam_maps]:
        if not path.exists():
            raise FileNotFoundError(path)

    source_rows, dev_selected = copy_assets(args, docs)
    write_map_pages(args, docs)
    write_index(docs, dev_selected)
    write_asset_readmes(docs, source_rows, dev_selected)

    print(f"Generated {len(FINAL_MAPS)} final map pages")
    print(f"Copied {len(source_rows)} final source maps")
    print(f"Selected {len(dev_selected)} development entries")
    print(f"Copied {sum(1 for record in dev_selected if record['source'])} development source maps")


if __name__ == "__main__":
    main()
