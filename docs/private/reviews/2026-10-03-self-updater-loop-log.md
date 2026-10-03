# RETR-0021 self-updater — cold-eyes loop log

Spec: `../specs/2026-10-03-self-updater.md`.

## Cold-eyes loop log

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|---|---|---|---|---|---|---|---|
| 1 | 2026-10-03 | 2 (`neutral-lane`; every lane held every question) | 3 | 2 | 2 | 1 | Verified 8, fixed 8, dismissed 2. One Q1 (`/proc/self/exe` wording) found while building the packet and fixed before dispatch. Fixed: test seam `self_update_endpoints` (Q4, both lanes); checksum read back from the staged file, no per-chunk hook (Q1, both); `C:\RetroArch-Win64` instead of unmeasured `%SystemDrive%` (Q1); parser takes optional `v`, 2-4 parts (Q2); `%TEMP%\retroarch-update\` cleaned at start-up (Q2); cross-host HTTPS redirects stated as followed (Q3, both); `HAVE_7ZIP` in the gate (Q3, from an open question). Dismissed: "download does not resume" (`task_http_resume` resumes a sink download; packet window was short); "HEAD unsupported" (orchestrator's own claim, refuted at 4a step 3: `net_http.c` treats a HEAD reply as body-less; rewrite reverted). Open questions resolved clean, none a finding: Windows restart fires; stable 7z layouts (single top folder; one depth-2 AppImage); nightly update pack = exe, DLLs, `filters/`, `platforms/`; renaming a loaded DLL and replacing a running AppImage both measured OK. Unrunnable here: Windows installer behaviour beyond the wintest measurements. Loop 2 owed (spec cap 2). |
