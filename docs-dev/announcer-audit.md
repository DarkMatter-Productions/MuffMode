# Announcer call-site audit

Maps every pre-migration `AnnouncerSound()` site to its triple `(stem, backup, use_backup)` and migration target.

| # | File:Line | Stem | Backup | use_backup | has_backup | Migration |
|---|-----------|------|--------|------------|------------|-----------|
| 1 | mm_vote.cpp:158 | vote_failed | null | false | no | MM_Announce(VoteFailed) |
| 2 | mm_vote.cpp:189 | vote_passed | null | false | no | MM_Announce(VotePassed) |
| 3 | mm_vote.cpp:195 | vote_failed | null | false | no | MM_Announce(VoteFailed) |
| 4 | mm_vote.cpp:1121 | vote_now | misc/pc_up.wav | true | yes | MM_Announce(VoteNow) |
| 5 | mm_match.cpp:405 | round_begins_in | null | false | no | MM_Announce(RoundBeginsIn) |
| 6 | mm_match.cpp:524 | fight | misc/tele_up.wav | true | yes | MM_Announce(FightWithBackup) |
| 7 | mm_match.cpp:849 | fight | null | false | no | MM_AnnounceRaw |
| 8 | mm_match.cpp:855 | fight | null | false | no | MM_AnnounceRaw |
| 9 | mm_match.cpp:879 | blue_wins_round | ctf/flagcap.wav | true | yes | MM_Announce(BlueWinsRound) |
| 10 | mm_match.cpp:886 | red_wins_round | ctf/flagcap.wav | false | yes | MM_Announce(RedWinsRound) |
| 11 | mm_match.cpp:947 | round_won | ctf/flagcap.wav | true | yes | MM_Announce(RoundWon) |
| 12 | mm_match.cpp:983 | round_won | ctf/flagcap.wav | true | yes | MM_Announce(RoundWon) |
| 13 | mm_match.cpp:1031 | round_won | ctf/flagcap.wav | true | yes | MM_Announce(RoundWon) |
| 14 | mm_match.cpp:1064 | red_wins_round | ctf/flagcap.wav | false | yes | MM_Announce(RedWinsRound) |
| 15 | mm_match.cpp:1069 | blue_wins_round | ctf/flagcap.wav | true | yes | MM_Announce(BlueWinsRound) |
| 16 | mm_match.cpp:1090 | red_wins_round | ctf/flagcap.wav | false | yes | MM_Announce(RedWinsRound) |
| 17 | mm_match.cpp:1095 | blue_wins_round | ctf/flagcap.wav | true | yes | MM_Announce(BlueWinsRound) |
| 18 | mm_match.cpp:1139 | null | world/{t}sec.wav | false | yes | MM_AnnounceRaw |
| 19 | mm_match.cpp:1143 | one/two/three | null | false | no | MM_Announce(One/Two/Three) |
| 20 | mm_match.cpp:1186 | null | world/{t}sec.wav | false | yes | MM_AnnounceRaw |
| 21 | mm_match.cpp:1191 | 5_minute/1_minute | null | false | no | MM_Announce(FiveMinute/OneMinute) |
| 22 | mm_match.cpp:1445 | prepare_* | null | false | no | MM_Announce(Prepare*) |
| 23 | mm_freezetag.cpp:1418 | blue/red_wins_round | ctf/flagcap.wav | team==BLUE | yes | MM_AnnounceRaw |
| 24 | items.cpp:1640 | dynamic | null | false | no | MM_Announce(event) |
| 25 | runtime.cpp:1375 | N_frag(s) | null | false | no | MM_AnnounceRaw |
| 26 | runtime.cpp:1416 | lead_tied/lead_taken | null | false | no | MM_Announce(LeadTied/LeadTaken) |
| 27 | runtime.cpp:1426 | lead_lost | null | false | no | MM_Announce(LeadLost) |
| 28 | runtime.cpp:1449 | blue/red_leads | null | false | no | MM_Announce(BlueLeads/RedLeads) |
| 29 | runtime.cpp:1452 | teams_tied | null | false | no | MM_Announce(TeamsTied) |
| 30 | runtime.cpp:1457 | blue/red_scores | null | false | no | MM_Announce(BlueScores/RedScores) |
| 31 | runtime.cpp:1776 | overtime | world/klaxon2.wav | true | yes | MM_Announce(Overtime) |
| 32 | runtime.cpp:1779 | sudden_death | world/klaxon2.wav | true | yes | MM_Announce(SuddenDeath) |
| 33 | runtime.cpp:2027 | red_wins/blue_wins | null | false | no | MM_Announce(RedWins/BlueWins) |
| 34 | runtime.cpp:2029 | you_win/you_lose | null | false | no | MM_Announce(YouWin/YouLose) |
| 35 | death.cpp:368 | rampage1 | null | false | no | MM_Announce(Rampage1) |
| 36 | death.cpp:667 | first_excellent | null | false | no | MM_Announce(FirstExcellent) |
| 37 | death.cpp:669 | excellent1 | null | false | no | MM_Announce(Excellent1) |
| 38 | death.cpp:678 | humiliation1 | null | false | no | MM_Announce(Humiliation1) |
| 39 | mm_strike.cpp:77 | red/blue_wins_round | ctf/flagcap.wav | attacker==BLUE | yes | MM_AnnounceRaw |
| 40 | mm_strike.cpp:102 | red/blue_wins_round | ctf/flagcap.wav | defender==BLUE | yes | MM_AnnounceRaw |
| 41 | mm_strike.cpp:112 | round_won | ctf/flagcap.wav | true | yes | MM_Announce(RoundWon) |
| 42 | mm_horde.cpp:460 | fight | null | false | no | MM_AnnounceRaw |
