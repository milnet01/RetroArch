# close-findings ledger, 2026-10-21 reviews

The run that closes the findings in this directory, split by subject in
the user's ruled order (RETR-0021, 2026-10-21 ruling 3). Fixes are on
`local/fixes-2026-09`; each commit body names the findings it closes.

| Group | State | Record |
|---|---|---|
| Save-file data loss | closed | `ledger-save-file.json`; RETR-0009, RETR-0021 notes |
| Cloud sync | closed: 13 fixed, 1 queued (RETR-0047), 1 out of scope queued (RETR-0048) | `ledger-cloud-sync.json` |
| Self-updater | closed | table below |
| Menu | closed: 21 fixed, 4 dismissed; next door queued (RETR-0049) | `ledger-menu.json` |
| Replay / input | closed: 18 fixed, 3 queued (RETR-0052, RETR-0053, RETR-0054), 8 dismissed; next door queued (RETR-0051) | `ledger-replay-input.json` |
| local-CI.sh | closed: 17 fixed (RETR-0050 among them), 1 dismissed; next door queued (RETR-0055) | `ledger-local-ci.json` |
| Rest | closed: 21 fixed (T1, T1a, T1b carried from upstream a1bba98b1d), 5 dismissed | `ledger-rest.json` |

## Self-updater dispositions

Sources: `review-code-leads/lane-01..03`, `review-readability-self-updater/message1.md`.

| Finding | Disposition |
|---|---|
| lane 1 su_undo (MEDIUM) | fixed 0a5adc7473 (INV-16, gated in spec loops 5-8; RETR-0042 closed) |
| lane 1 A2 quoted UninstallString | fixed 4a8a64a797; REG_EXPAND_SZ half queued RETR-0043 |
| lane 1 A3 marker remove | fixed ad87ccbab7 |
| lane 1 A5 struct order | fixed 629120f883 |
| lane 2 B1 marker before launch | fixed 4892aecf3b |
| lane 2 B2, B3 wrong reasons | fixed 97da605d57 |
| lane 2 C1 silent Update Now failures | fixed a112265398 |
| lane 2 blank page during start-up check | fixed b461ef3c03 |
| lane 3 D1 reset doc; readability L1 3, 4 | fixed db663de23b |
| lane 3 quadratic folder walk | queued RETR-0044 |
| readability L1 1 (spec citations), 5 | fixed 083fc755e0 |
| readability L1 2 staging names | fixed ff68d28504 |
| readability L1 6, 7 | fixed 3acbe7a0f6 |
| readability L2 1 per-kind facts | fixed 8321416e38 |
| readability L2 2, 4, 5, 6 | fixed 3e3ba1ef3f |
| readability L2 3 TLS notice | dismissed: user ruling 2026-10-21 (1) of review-code, start-up check stays silent |
| readability L2 7 `preferred` name | queued RETR-0045 |
| readability L2 8 empty-entry rule restated | dismissed: each site needs the `e->size` test, and the project rule forbids one-line accessor functions |
| readability L2 9 marker folder | fixed bab1865a96 |
| readability L2 10, 11 | fixed 08b5a539a8 |
| readability message2 part 3 leads | queued RETR-0046 (unverified leads) |

Sweep (self-updater): every changed symbol searched tree-wide
(`menu_content_downloads_record`, staging names, `archive_size`); only
the dated review returns name the old ones (frozen). Gates: full
`make -j4 retroarch`, `self_update_test`, `self_update_net_test`
(79 checks), `content_downloads_test`, all passed; local CI passed on
push of 08b5a539a8.

Upstream PR candidates: see RETR-S0115's 2026-10-21 note.
