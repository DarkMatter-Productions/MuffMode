# Muff Mode Map Guide

[README](../../README.md) | [Gameplay Reference](../gameplay-reference.md) | [Level Design Guide](../level-design-guide.md)

Muff Mode's `mm-*` maps are a small museum with live rockets in it: Quake II duel standards, late-90s community curios, Rocket Arena and Quake III visitors, and a few stranger public-server pieces rebuilt for the rerelease era.

This guide covers the current authoritative final set from `mm-maps-finals-1`. Each detail page includes the Muff Mode filename, release status, original release date where it could be found, a short history, play notes, preserved files, and a level item register read from the final BSP. Work-in-progress coverage is limited to the active local dev tree and keeps only the newest apparent state for each map identity.

## Status Key

| Suffix | Meaning |
| --- | --- |
| No suffix | Final release |
| `-rcN` | Release candidate N |
| `-bN` | Beta N |
| `-aN` | Alpha N |
| `-buildN` | Internal work build N |

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

These entries are copied only from `MuffMode-Map-Remasters/dev`. Older states for the same map identity are omitted.

| Map | Filename | Status | Source map | BSP in dev folder |
| --- | --- | --- | --- | --- |
| Ad Mortem | `mm-admortem-rc1` | Release candidate 1 | [mm-admortem-rc1.map](dev-source-maps/mm-admortem-rc1.map) | No |
| Aggressor | `mm-aggressor-rc3` | Release candidate 3 | [mm-aggressor-rc3.map](dev-source-maps/mm-aggressor-rc3.map) | Yes |
| Almost Lost | `mm-almostlost-b1` | Beta 1 | [mm-almostlost-b1.map](dev-source-maps/mm-almostlost-b1.map) | No |
| Arena of Death | `mm-arena-b1` | Beta 1 | [mm-arena-b1.map](dev-source-maps/mm-arena-b1.map) | Yes |
| Asylum | `mm-asylum-build` | Work build | [mm-asylum-build.map](dev-source-maps/mm-asylum-build.map) | No |
| Basewalk | `mm-basewalk-build2` | Work build 2 | [mm-basewalk-build2.map](dev-source-maps/mm-basewalk-build2.map) | No |
| Battleforged | `mm-bttlforged-build` | Work build | [mm-bttlforged-build.map](dev-source-maps/mm-bttlforged-build.map) | No |
| Bravado | `mm-bravado-build` | Work build | [mm-bravado-build.map](dev-source-maps/mm-bravado-build.map) | No |
| Burn Cycle | `mm-burncycle-rc2` | Release candidate 2 | [mm-burncycle-rc2.map](dev-source-maps/mm-burncycle-rc2.map) | No |
| Colours of War | `mm-coloursofwar-rc2` | Release candidate 2 | [mm-coloursofwar-rc2.map](dev-source-maps/mm-coloursofwar-rc2.map) | No |
| Cunning Plan | `mm-cunningp-rc2` | Release candidate 2 | [mm-cunningp-rc2.map](dev-source-maps/mm-cunningp-rc2.map) | No |
| Deep Inside | `mm-deepinside-build` | Work build | [mm-deepinside-build.map](dev-source-maps/mm-deepinside-build.map) | No |
| Deva Station | `mm-devastation-b1` | Beta 1 | [mm-devastation-b1.map](dev-source-maps/mm-devastation-b1.map) | No |
| Envy Flows | `mm-envyflos-rc1` | Release candidate 1 | [mm-envyflos-rc1.map](dev-source-maps/mm-envyflos-rc1.map) | No |
| Evolution | `mm-evolution-a1` | Alpha 1 | [mm-evolution-a1.map](dev-source-maps/mm-evolution-a1.map) | Yes |
| Eye to Eye | `mm-eye-build` | Work build | [mm-eye-build.map](dev-source-maps/mm-eye-build.map) | No |
| Gothic Revenge | `mm-gothic-rc1` | Release candidate 1 | [mm-gothic-rc1.map](dev-source-maps/mm-gothic-rc1.map) | No |
| Grim Remains | `mm-grimremains-a1` | Alpha 1 | [mm-grimremains-a1.map](dev-source-maps/mm-grimremains-a1.map) | No |
| Hard Angels | `mm-hardangels-rc2` | Release candidate 2 | [mm-hardangels-rc2.map](dev-source-maps/mm-hardangels-rc2.map) | No |
| Hidden Fortress | `mm-hiddenfortress-b1` | Beta 1 | [mm-hiddenfortress-b1.map](dev-source-maps/mm-hiddenfortress-b1.map) | No |
| Hook in Mouth | `mm-hook-rc1` | Release candidate 1 | [mm-hook-rc1.map](dev-source-maps/mm-hook-rc1.map) | Yes |
| House of Decay | `mm-housedecay-a1` | Alpha 1 | [mm-housedecay-a1.map](dev-source-maps/mm-housedecay-a1.map) | Yes |
| Moebius Trip | `mm-moebiustrip-rc1` | Release candidate 1 | [mm-moebiustrip-rc1.map](dev-source-maps/mm-moebiustrip-rc1.map) | No |
| One Must Fall | `mm-onemayfall-rc1` | Release candidate 1 | [mm-onemayfall-rc1.map](dev-source-maps/mm-onemayfall-rc1.map) | Yes |
| Overkill | `mm-overkill-a2` | Alpha 2 | [mm-overkill-a2.map](dev-source-maps/mm-overkill-a2.map) | Yes |
| Phrantic | `mm-phrantic-a1` | Alpha 1 | [mm-phrantic-a1.map](dev-source-maps/mm-phrantic-a1.map) | Yes |
| Psychosis Fixation | `mm-psychosis-rc1` | Release candidate 1 | [mm-psychosis-rc1.map](dev-source-maps/mm-psychosis-rc1.map) | Yes |
| Quarantine | `mm-quarantine-build` | Work build | [mm-quarantine-build.map](dev-source-maps/mm-quarantine-build.map) | No |
| Reckless Abandon | `mm-reckless-rc1` | Release candidate 1 | [mm-reckless-rc1.map](dev-source-maps/mm-reckless-rc1.map) | No |
| Retribution | `mm-retribution-build` | Work build | [mm-retribution-build.map](dev-source-maps/mm-retribution-build.map) | No |
| REVENGE 3 panza@tic.de | `mm-revenge-rc1` | Release candidate 1 | [mm-revenge-rc1.map](dev-source-maps/mm-revenge-rc1.map) | No |
| Sandstone Crypt | `mm-sandstone-rc1` | Release candidate 1 | [mm-sandstone-rc1.map](dev-source-maps/mm-sandstone-rc1.map) | No |
| shifter | `mm-shifter-a1` | Alpha 1 | [mm-shifter-a1.map](dev-source-maps/mm-shifter-a1.map) | Yes |
| Solid | `mm-solid-build` | Work build | [mm-solid-build.map](dev-source-maps/mm-solid-build.map) | No |
| Space Chamber | `mm-spacech-build` | Work build | [mm-spacech-build.map](dev-source-maps/mm-spacech-build.map) | No |
| SPEG 999 | `mm-speg-rc1` | Release candidate 1 | [mm-speg-rc1.map](dev-source-maps/mm-speg-rc1.map) | No |
| The Bad Place | `mm-badplace-rc3` | Release candidate 3 | [mm-badplace-rc3.map](dev-source-maps/mm-badplace-rc3.map) | Yes |
| The Bouncy Map | `mm-bouncy-a1` | Alpha 1 | [mm-bouncy-a1.map](dev-source-maps/mm-bouncy-a1.map) | No |
| The Chastity Belt | `mm-chastitybelt-build2` | Work build 2 | [mm-chastitybelt-build2.map](dev-source-maps/mm-chastitybelt-build2.map) | No |
| The Cistern | `mm-cistern-build2` | Work build 2 | [mm-cistern-build2.map](dev-source-maps/mm-cistern-build2.map) | No |
| The Dark Zone | `mm-darkzone-rc1` | Release candidate 1 | No selected source map located | Yes |
| The DredWerkz | `mm-dredwerkz-build` | Work build | [mm-dredwerkz-build.map](dev-source-maps/mm-dredwerkz-build.map) | No |
| The Forgotten Place | `mm-forgottenplace-a1` | Alpha 1 | [mm-forgottenplace-a1.map](dev-source-maps/mm-forgottenplace-a1.map) | Yes |
| The Fragging Yard 1v1 | `mm-fraggingyard-a1` | Alpha 1 | [mm-fraggingyard-a1.map](dev-source-maps/mm-fraggingyard-a1.map) | Yes |
| The Fury | `mm-fury-rc2` | Release candidate 2 | [mm-fury-rc2.map](dev-source-maps/mm-fury-rc2.map) | Yes |
| The Hunt | `mm-hunt-rc1` | Release candidate 1 | [mm-hunt-rc1.map](dev-source-maps/mm-hunt-rc1.map) | Yes |
| The Nameless Place | `mm-nameless-build1` | Work build 1 | [mm-nameless-build1.map](dev-source-maps/mm-nameless-build1.map) | No |
| The Proving Grounds | `mm-provinggrounds-a5` | Alpha 5 | [mm-provinggrounds-a5.map](dev-source-maps/mm-provinggrounds-a5.map) | No |
| The Vomitorium | `mm-vomitorium-rc1` | Release candidate 1 | [mm-vomitorium-rc1.map](dev-source-maps/mm-vomitorium-rc1.map) | No |
| Theatre of Pain | `mm-tpain-a1` | Alpha 1 | [mm-tpain-a1.map](dev-source-maps/mm-tpain-a1.map) | Yes |
| Torment | `mm-torm-rc1` | Release candidate 1 | [mm-torm-rc1.map](dev-source-maps/mm-torm-rc1.map) | No |
| Toxicity | `mm-toxicity-build` | Work build | [mm-toxicity-build.map](dev-source-maps/mm-toxicity-build.map) | No |
| Trinity | `mm-trinity-build` | Work build | [mm-trinity-build.map](dev-source-maps/mm-trinity-build.map) | No |
| Ubiquity | `mm-ubiquity-rc1` | Release candidate 1 | [mm-ubiquity-rc1.map](dev-source-maps/mm-ubiquity-rc1.map) | No |
| Under Pressure | `mm-upress-rc1` | Release candidate 1 | [mm-upress-rc1.map](dev-source-maps/mm-upress-rc1.map) | No |
| Vertical Vengeance | `mm-verticalv-b1` | Beta 1 | [mm-verticalv-b1.map](dev-source-maps/mm-verticalv-b1.map) | Yes |
| Worn and Torn | `mm-worn-rc1` | Release candidate 1 | [mm-worn-rc1.map](dev-source-maps/mm-worn-rc1.map) | No |

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
- DondeQ2's map collection helped cross-check community map names and authors: [WEBMAN and twists Quake 2 Map Collection](https://dondeq2.com/2017/10/24/webman-twists-map-collection/).
- Additional history came from [PlanetQuake's Cold Zero Level of the Week](https://planetquake.gamespy.com/View38ff.html?id=177&view=LOTW.Detail), [Internet Archive's Quake II Netpack I: Extremities](https://archive.org/details/QuakeIINetpackIExtremitiesUSAEurope), [LvLWorld's Wicked readme](https://lvlworld.com/readme/id%3A701), [LvLWorld's MKSTEEL readme](https://lvlworld.com/readme/id%3A1250), [Daniel Nolan's Quake 2 maps page](https://dnolan.com/quake-2/), and Quake/Rocket Arena community listings linked from the individual pages.
