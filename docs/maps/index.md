# Muff Mode Map Guide

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

| Mood | Start with |
| --- | --- |
| Duel pressure | [Aerowalk](mm-aerow.md), [The Rage](mm-rage.md), [Painkiller](mm-pkill.md), [Mortal Coil](mm-mcoil.md), [Wicked](mm-wicked.md) |
| Public FFA or TDM | [Conventional](mm-conven.md), [Degeneration](mm-degen.md), [Lava Lamp](mm-llamp.md), [Unknown Domain](mm-undom.md), [The Oppressor](mm-oppress.md) |
| Rail and aim night | [Railgun 101](mm-rail101.md), [Thunderstruck](mm-thunders.md), [Window Pain](mm-winpain.md), [The Longest Yard](mm-longyd.md) |
| ztn classics | [The Crucible](mm-crucible.md), [The Killing Machine](mm-kmach.md), [Painkiller](mm-pkill.md), [The Rage](mm-rage.md) |
| Cross-game guests | [The Longest Yard](mm-longyd.md), [Wicked](mm-wicked.md), [Thunderstruck](mm-thunders.md), [Window Pain](mm-winpain.md) |

## Final Maps

| Map | Filename | Original lineage | Original release date | Good fits |
| --- | --- | --- | --- | --- |
| [Aerowalk](mm-aerow.md) | `mm-aerow` | aeroq2 / Aerowalk | 11 April 1998 | Duel, small FFA, 2v2, Clan Arena |
| [Bio Rust](mm-biorust.md) | `mm-biorust` | koldduel1 / Bio Rust | 12 October 2008 | Duel, small FFA, 2v2 |
| [Conventional](mm-conven.md) | `mm-conven` | grom_dm3 / Conventional | 4 November 1999 | FFA, 2v2, TDM, Quad Hog |
| [The Crucible](mm-crucible.md) | `mm-crucible` | ztn2dm5 / The Crucible | 6 September 1998 | Duel, FFA, 2v2 |
| [Cold Zero](mm-czero.md) | `mm-czero` | ven_dm2 / Cold Zero | 22 April 2001 | FFA, 2v2, TDM, Instagib |
| [Degeneration](mm-degen.md) | `mm-degen` | paradm4 / Degeneration | 14 June 1999 | FFA, 2v2, TDM |
| [The Flesh Refinery](mm-fleshref.md) | `mm-fleshref` | fleshref / q2rdm1 | 13 February 2001 | Duel, small FFA, Power Screen experiment |
| [Grind](mm-grind.md) | `mm-grind` | grind / q2duel2 | 17 March 1998 | Duel, 2v2, FFA |
| [Iron Oxide](mm-ironox.md) | `mm-ironox` | ktdm1 / Iron Oxide | 18 September 1999 | Duel, small FFA, 2v2 |
| [The Killing Machine](mm-kmach.md) | `mm-kmach` | ztn2dm2 / kmachine | 22 March 1998 | FFA, 2v2, casual Duel |
| [Lava Lamp](mm-llamp.md) | `mm-llamp` | 4u2map12a / Lava Lamp | 2003 (exact day not located) | FFA, TDM, party server |
| [The Longest Yard](mm-longyd.md) | `mm-longyd` | q3dm17 / The Longest Yard | 2 December 1999 | FFA, Instagib, Clan Arena, jump-pad chaos |
| [Mortal Coil](mm-mcoil.md) | `mm-mcoil` | broken2 / q2duel5 | 24 April 1998 | Duel, small FFA, 2v2 |
| [Negative Impulse](mm-negimp.md) | `mm-negimp` | vd6dm2 / Negative Impulse | Not located | FFA, 2v2, TDM |
| [The Oppressor](mm-oppress.md) | `mm-oppress` | msdm5 / The Oppressor | 5 August 1998 | FFA, TDM, 2v2 |
| [Painkiller](mm-pkill.md) | `mm-pkill` | ztn2dm1 / painklr2 | 22 January 1998 | Duel, small FFA, Clan Arena |
| [The Rage](mm-rage.md) | `mm-rage` | ztn2dm3 / q2duel8 | 10 April 1998 | Duel, FFA, 2v2 |
| [Railgun 101](mm-rail101.md) | `mm-rail101` | 2box4 / Railgun 101 | 23 May 1998 | Instagib, rail practice, aim warmups |
| [Reclamation](mm-reclam.md) | `mm-reclam` | kamq2dm3 / q2rdm7 | 4 March 1999 | Duel, small FFA |
| [Thunderstruck](mm-thunders.md) | `mm-thunders` | ra3map1 / Thunderstruck | 24 July 2000 | Duel, Clan Arena, Instagib, rail/rocket practice |
| [Unknown Domain](mm-undom.md) | `mm-undom` | trdm04a / Unknown Domain | Not located | FFA, TDM, large public play |
| [Wicked](mm-wicked.md) | `mm-wicked` | cpm1a / Wicked | 3 January 2000; rereleased 23 October 2000 | Duel, Clan Arena, small FFA |
| [Window Pain](mm-winpain.md) | `mm-winpain` | ra3map2c / Window Pain | 24 July 2000 | Clan Arena, Instagib, FFA warmups |

## In Development

These entries are copied only from `MuffMode-Map-Remasters/dev`. Older states for the same map identity are omitted. Internal work snapshots and entries without both a source `.map` and compiled `.bsp` are excluded.

| Map | Filename | Status | Source map | BSP in dev folder |
| --- | --- | --- | --- | --- |
| Aggressor | `mm-aggressor-rc3` | Release candidate 3 | [mm-aggressor-rc3.map](dev-source-maps/mm-aggressor-rc3.map) | Yes |
| Almost Lost | `mm-almostlost-a1` | Alpha 1 | [mm-almostlost-a1.map](dev-source-maps/mm-almostlost-a1.map) | Yes |
| Arena of Death | `mm-arena-b1` | Beta 1 | [mm-arena-b1.map](dev-source-maps/mm-arena-b1.map) | Yes |
| Evolution | `mm-evolution-a1` | Alpha 1 | [mm-evolution-a1.map](dev-source-maps/mm-evolution-a1.map) | Yes |
| Hook in Mouth | `mm-hook-rc1` | Release candidate 1 | [mm-hook-rc1.map](dev-source-maps/mm-hook-rc1.map) | Yes |
| House of Decay | `mm-housedecay-a1` | Alpha 1 | [mm-housedecay-a1.map](dev-source-maps/mm-housedecay-a1.map) | Yes |
| One Must Fall | `mm-onemayfall-rc1` | Release candidate 1 | [mm-onemayfall-rc1.map](dev-source-maps/mm-onemayfall-rc1.map) | Yes |
| Overkill | `mm-overkill-a2` | Alpha 2 | [mm-overkill-a2.map](dev-source-maps/mm-overkill-a2.map) | Yes |
| Phrantic | `mm-phrantic-a1` | Alpha 1 | [mm-phrantic-a1.map](dev-source-maps/mm-phrantic-a1.map) | Yes |
| Psychosis Fixation | `mm-psychosis-rc1` | Release candidate 1 | [mm-psychosis-rc1.map](dev-source-maps/mm-psychosis-rc1.map) | Yes |
| shifter | `mm-shifter-a1` | Alpha 1 | [mm-shifter-a1.map](dev-source-maps/mm-shifter-a1.map) | Yes |
| The Bad Place | `mm-badplace-rc3` | Release candidate 3 | [mm-badplace-rc3.map](dev-source-maps/mm-badplace-rc3.map) | Yes |
| The Forgotten Place | `mm-forgottenplace-a1` | Alpha 1 | [mm-forgottenplace-a1.map](dev-source-maps/mm-forgottenplace-a1.map) | Yes |
| The Fragging Yard 1v1 | `mm-fraggingyard-a1` | Alpha 1 | [mm-fraggingyard-a1.map](dev-source-maps/mm-fraggingyard-a1.map) | Yes |
| The Fury | `mm-fury-rc2` | Release candidate 2 | [mm-fury-rc2.map](dev-source-maps/mm-fury-rc2.map) | Yes |
| The Hunt | `mm-hunt-rc1` | Release candidate 1 | [mm-hunt-rc1.map](dev-source-maps/mm-hunt-rc1.map) | Yes |
| The Proving Grounds | `mm-proving-a4` | Alpha 4 | [mm-proving-a4.map](dev-source-maps/mm-proving-a4.map) | Yes |
| The Vomitorium | `mm-vomitorium-b1` | Beta 1 | [mm-vomitorium-b1.map](dev-source-maps/mm-vomitorium-b1.map) | Yes |
| Theatre of Pain | `mm-tpain-a1` | Alpha 1 | [mm-tpain-a1.map](dev-source-maps/mm-tpain-a1.map) | Yes |
| Vertical Vengeance | `mm-verticalv-b1` | Beta 1 | [mm-verticalv-b1.map](dev-source-maps/mm-verticalv-b1.map) | Yes |

## Preserved Files

Original BSPs are kept under [original-bsps](original-bsps/README.md), original readmes under [original-readmes](original-readmes/README.md), and remaster source `.map` files under [source-maps](source-maps/README.md). The source maps are deliberately separate from BSP files.

| Map | Original readme | Original BSP | Remaster source map |
| --- | --- | --- | --- |
| [Aerowalk](mm-aerow.md) | [aerowalk-readme.txt](original-readmes/aerowalk-readme.txt) | [aerowalk.bsp](original-bsps/aerowalk.bsp) | [mm-aerow.map](source-maps/mm-aerow.map) |
| [Bio Rust](mm-biorust.md) | Not located | [koldduel1.bsp](original-bsps/koldduel1.bsp) | [mm-biorust.map](source-maps/mm-biorust.map) |
| [Conventional](mm-conven.md) | Not located | [grom_dm3.bsp](original-bsps/grom_dm3.bsp) | [mm-conven.map](source-maps/mm-conven.map) |
| [The Crucible](mm-crucible.md) | [ztn2dm5-readme.txt](original-readmes/ztn2dm5-readme.txt) | [ztn2dm5.bsp](original-bsps/ztn2dm5.bsp) | [mm-crucible.map](source-maps/mm-crucible.map) |
| [Cold Zero](mm-czero.md) | Not located | [ven_dm2.bsp](original-bsps/ven_dm2.bsp) | [mm-czero.map](source-maps/mm-czero.map) |
| [Degeneration](mm-degen.md) | Not located | [paradm4.bsp](original-bsps/paradm4.bsp) | [mm-degen.map](source-maps/mm-degen.map) |
| [The Flesh Refinery](mm-fleshref.md) | [fleshref-readme.txt](original-readmes/fleshref-readme.txt) | [fleshref.bsp](original-bsps/fleshref.bsp) | [mm-fleshref.map](source-maps/mm-fleshref.map) |
| [Grind](mm-grind.md) | [grind-readme.txt](original-readmes/grind-readme.txt) | [grind.bsp](original-bsps/grind.bsp) | [mm-grind.map](source-maps/mm-grind.map) |
| [Iron Oxide](mm-ironox.md) | Not located | [ktdm1.bsp](original-bsps/ktdm1.bsp) | [mm-ironox.map](source-maps/mm-ironox.map) |
| [The Killing Machine](mm-kmach.md) | [ztn2dm2-readme.txt](original-readmes/ztn2dm2-readme.txt) | [ztn2dm2.bsp](original-bsps/ztn2dm2.bsp) | [mm-kmach.map](source-maps/mm-kmach.map) |
| [Lava Lamp](mm-llamp.md) | Not located | [4u2map12a.bsp](original-bsps/4u2map12a.bsp) | [mm-llamp.map](source-maps/mm-llamp.map) |
| [The Longest Yard](mm-longyd.md) | Not located | Not preserved locally | [mm-longyd.map](source-maps/mm-longyd.map) |
| [Mortal Coil](mm-mcoil.md) | [broken2-readme.txt](original-readmes/broken2-readme.txt) | [broken2.bsp](original-bsps/broken2.bsp) | [mm-mcoil.map](source-maps/mm-mcoil.map) |
| [Negative Impulse](mm-negimp.md) | Not located | [vd6dm2.bsp](original-bsps/vd6dm2.bsp) | [mm-negimp.map](source-maps/mm-negimp.map) |
| [The Oppressor](mm-oppress.md) | Not located | [msdm5.bsp](original-bsps/msdm5.bsp) | [mm-oppress.map](source-maps/mm-oppress.map) |
| [Painkiller](mm-pkill.md) | [ztn2dm1-readme.txt](original-readmes/ztn2dm1-readme.txt) | [ztn2dm1.bsp](original-bsps/ztn2dm1.bsp) | [mm-pkill.map](source-maps/mm-pkill.map) |
| [The Rage](mm-rage.md) | [ztn2dm3-readme.txt](original-readmes/ztn2dm3-readme.txt) | [ztn2dm3.bsp](original-bsps/ztn2dm3.bsp) | [mm-rage.map](source-maps/mm-rage.map) |
| [Railgun 101](mm-rail101.md) | [2box4-readme.txt](original-readmes/2box4-readme.txt) | [2box4.bsp](original-bsps/2box4.bsp) | [mm-rail101.map](source-maps/mm-rail101.map) |
| [Reclamation](mm-reclam.md) | Not located | [kamq2dm3.bsp](original-bsps/kamq2dm3.bsp) | [mm-reclam.map](source-maps/mm-reclam.map) |
| [Thunderstruck](mm-thunders.md) | Not located | Not preserved locally | [mm-thunders.map](source-maps/mm-thunders.map) |
| [Unknown Domain](mm-undom.md) | Not located | [trdm04a.bsp](original-bsps/trdm04a.bsp) | [mm-undom.map](source-maps/mm-undom.map) |
| [Wicked](mm-wicked.md) | Not located | Not preserved locally | [mm-wicked.map](source-maps/mm-wicked.map) |
| [Window Pain](mm-winpain.md) | Not located | Not preserved locally | [mm-winpain.map](source-maps/mm-winpain.map) |

## Research Notes

- Local final BSPs and source maps provided the included filenames, final status, worldspawn titles, and entity/item counts.
- Preserved local readmes supplied exact dates and author notes for Aerowalk, Railgun 101, the ztn maps, Mortal Coil, The Flesh Refinery, and Grind.
- The Steam Quake II rerelease installation supplied several original community BSPs where readmes were not present locally.
- Internet Archive and Wayback Machine research added several useful archive trails: [Quake II Netpack I: Extremities](https://archive.org/details/QuakeIINetpackIExtremitiesUSAEurope), the 2003 [How Delightful custom-map pack](https://web.archive.org/web/20031227060529/http://hd.ausgamers.com:80/main.html) and [mapcfg](https://web.archive.org/web/20040104011615if_/http://hd.ausgamers.com:80/mapcfg.txt), the [QuakeUnity site rip](https://archive.org/details/quakeunity.com), and Archive.org-hosted EDL finals videos linked from the individual ztn and Mortal Coil pages.
- DondeQ2's [WEBMAN and twists Quake 2 Map Collection](https://dondeq2.com/2017/10/24/webman-twists-map-collection/), Q2Scene's [DemoSquad files](https://q2scene.com/ds/index.php?op=files), PlanetQuake Level of the Week pages, LvLWorld readmes, and author pages filled in names where Archive.org had only filename-level evidence.
- A few maps still have deliberately modest history notes because no direct original readme or first-class Archive.org item was found for them. Those pages say so rather than pretending a weak mirror listing is a primary source.
