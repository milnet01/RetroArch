# TLS Verification — Opt-In with Hostile-WiFi Warning (Design)

**Date:** 2026-04-27
**Source:** `docs/private/ROADMAP.md` line 170 — indie-review CRITICAL.
**Status:** draft, awaiting user review
**Target:** `local/fixes-2026-04` (RA-side opt-in) + upstream PR (vendored mbedtls helper)
**Effort estimate:** 2–3 days RA-side; upstream coordination is open-ended.

---

## Summary

`libretro-common/net/net_socket_ssl_mbed.c:199` configures every outbound TLS connection with `MBEDTLS_SSL_VERIFY_OPTIONAL`. The mode does technically run certificate validation — the result lands in `flags` at line 220 — but the result is then formatted into a 512-byte buffer (`vrfy_buf`) that is **never read**, never logged, and never gates the connection. Any cert failure (expired, self-signed, hostname-mismatch, chain-broken) returns the socket fd to the caller anyway; the TLS handshake completes against any server presenting any cert.

This means **every HTTPS request from RetroArch is unauthenticated against MITM**:

- Cloud sync — Google Drive OAuth + Drive API, S3 SigV4 signing, WebDAVS.
- Cheevos — RetroAchievements API, login + score-submit.
- Online updater — core / asset / shader downloads.

On hostile WiFi (hotel, café, conference), an attacker with ARP-spoof or DNS-poison primitives can present any cert, capture Google OAuth refresh tokens, AWS secret access keys, and RA credentials in cleartext after their MITM endpoint terminates the connection. The RA client never notices.

This spec defines a phased fix: **make verification mandatory by default**, expose a documented opt-out for users with broken intermediate CA bundles or self-signed enterprise proxies, and log a loud warning when the opt-out is engaged.

## Current behaviour

```c
/* libretro-common/net/net_socket_ssl_mbed.c:199 */
mbedtls_ssl_conf_authmode(&state->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
mbedtls_ssl_conf_ca_chain(&state->conf, &state->ca, NULL);
/* ... */
if ((flags = mbedtls_ssl_get_verify_result(&state->ctx)) != 0)
{
   char vrfy_buf[512];
   mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);
   /* vrfy_buf is never read; flags is dropped on the floor. */
}

return state->net_ctx.fd;   /* ← fd returned regardless of verify result */
```

The CA bundle (`state->ca`) is loaded from the bundled `cacert.pem` — the same Mozilla NSS root list that curl ships. So the CA-chain validation **is correct in principle**; the bug is purely that the result is discarded.

This file is **vendored from libretro-common**, not RA-owned. Fixing it locally without coordinating upstream creates merge debt every time libretro-common is resynced.

## Goals (v1)

1. **Verification is mandatory by default.** New installs reject self-signed, expired, hostname-mismatched, or chain-broken certs. The TLS handshake fails cleanly with a logged error; the caller sees a connect failure, not a silent MITM.
2. **Documented opt-out for the corner cases that legitimately need it:**
   - Enterprise users behind a TLS-intercepting proxy with a custom corporate root CA.
   - Users on systems where the bundled `cacert.pem` is outdated and rotation is blocked (e.g. embedded targets that ship infrequent updates).
   - Local dev / CI hitting a self-signed staging endpoint.
3. **The opt-out is loud.** When enabled, every HTTPS connect logs `RARCH_WARN("[TLS] Certificate verification disabled — connections vulnerable to MITM. See settings/tls/verification.")` to the runloop message queue + stderr. The opt-out flag is **per-session**, not persisted, and resets to "verify" on RetroArch restart unless the user re-engages it explicitly. *(Decision point D3 below.)*
4. **Upstream the libretro-common fix.** Submit a PR to libretro/libretro-common adding the `MBEDTLS_SSL_VERIFY_REQUIRED` mode + a configurable opt-out hook. Pin RA's local fix to a specific upstream commit so resyncs don't regress.
5. **No breaking change for users who were silently being MITM'd.** They were already getting cert-rejected by *some* sites (any site whose cert legitimately failed validation against the bundled CA bundle was passing through silently — but their app worked because the bundle is up to date for ~99.5% of public CAs). Post-patch, that 0.5% sees connect failures. **Migration:** ship the patch with a clear release note + the opt-out path documented.

## Non-goals (v1)

- **Per-host opt-out granularity.** v1 is "all-on" or "all-off." Per-domain whitelisting (allow self-signed only for `192.168.1.10`) is v2 — adds a UI surface, persistence, and a migration path for the whitelist that v1 doesn't need.
- **Certificate pinning.** Pinning is a separate threat model (compromised CA) and would interact with the opt-out flag in non-obvious ways.
- **Forcing TLS 1.3.** The mbedtls config currently allows TLS 1.2+; this spec doesn't change that.
- **Replacing mbedtls.** Out of scope — the libretro-common decision to vendor mbedtls is upstream's call.
- **Cheevos-side soft fail.** The cheevos client already has its own connect-failure handling; v1 doesn't change cheevos retry logic, just lets connect failures propagate.

## Architecture

```
                                                   ┌─────────────────┐
  RA → ssl_socket_connect()                         │ settings        │
        │                                           │ tls_verify_mode │
        ├── read settings.tls_verify_mode ──────────┤   = "required"  │
        │                                           │   | "optional"  │
        │                                           │   | "disabled"  │
        ├── if required: VERIFY_REQUIRED            └─────────────────┘
        ├── if optional: VERIFY_OPTIONAL + log on failure
        └── if disabled: VERIFY_NONE + WARN on every connect
              ↓
        mbedtls_ssl_conf_authmode(...)
              ↓
        handshake — succeeds iff verify is satisfied (REQUIRED) /
                    always succeeds + WARN (OPTIONAL fail or DISABLED)
```

## Decision points

### D1 — Three modes or two?

- **(A) Two:** *required* (default) and *disabled* (opt-out).
- **(B) Three:** *required* (default), *optional* (verify but allow on failure with a warning — current behaviour, kept for users who genuinely want a half-step), *disabled* (skip verification entirely).

**Recommendation:** (B). The middle tier (*optional*) preserves the current behaviour exactly, which makes the v1 patch a non-breaking change for users who don't want to think about TLS — they get the warning but their connections still work. A second patch can deprecate *optional* once the warnings have been visible long enough to flag broken CA bundles. The deprecation path is itself a v2 spec.

### D2 — Where is the setting?

- **(A) `Settings → Network → TLS Verification`.** Visible to all users.
- **(B) `Settings → Network → Advanced → TLS Verification`.** Hidden behind the advanced toggle so casual users don't accidentally disable it.
- **(C) Config-file only (`retroarch.cfg::tls_verify_mode`), no UI.** Forces the user to RTFM.

**Recommendation:** (B). v1 default is *required*; the opt-out is a footgun for casual users and a non-issue for the enterprise / dev users who legitimately need it (they'll find the advanced setting). (C) is hostile — users on broken CA bundles need a discoverable path back to working downloads.

### D3 — Persistence of *disabled* mode?

- **(A) Persists in `retroarch.cfg`** like every other setting. User flips once, stays disabled forever.
- **(B) Resets to *required* on every RetroArch restart.** Forces re-engagement each session.
- **(C) Persists, but with a forced re-confirm prompt every N days / launches.**

**Recommendation:** (A) + a one-time consent dialog the first time *disabled* is selected. The dialog says: *"Disabling TLS verification means RetroArch will trust any server certificate. This makes you vulnerable to credential theft on untrusted networks (hotel WiFi, café, etc). Continue?"* with [Yes, disable] / [Cancel] buttons.  Once consented, the setting persists silently — but every connect logs the warning. (B) is hostile to enterprise users with stable corporate proxies. (C) is the worst of both — the user's already-consented setting nags them back into a dialog they've already passed.

### D4 — How is the *opt-out* warning surfaced?

- **(A) Per-connect log only** (`RARCH_WARN` to stderr / log file).
- **(B) Persistent on-screen indicator** while the menu is up — "TLS verification disabled" badge in the corner.
- **(C) On-screen toast on first connect** of each session.

**Recommendation:** (A) + (C). Per-connect log gives the audit trail; first-connect toast gives the user one chance to notice "oh right, I have this disabled." A persistent badge (B) is too aggressive — users who legitimately need disabled mode will hate it and screenshot-dox themselves accidentally.

## Data model changes

### `configuration.h`

```c
/* Existing nearby: bools.audio_enable, etc. */
struct
{
   /* ... */
   unsigned tls_verify_mode;   /* enum tls_verify_mode_t — see below */
   /* ... */
} uints;
```

```c
/* network/net_compat.h or new network/tls_config.h */
enum tls_verify_mode_t
{
   TLS_VERIFY_REQUIRED  = 0,   /* default */
   TLS_VERIFY_OPTIONAL  = 1,   /* mbedtls VERIFY_OPTIONAL + log on failure */
   TLS_VERIFY_DISABLED  = 2    /* mbedtls VERIFY_NONE + warn every connect */
};
```

`TLS_VERIFY_REQUIRED = 0` so unset / default-init / fresh config all land on the safe value.

### `libretro-common/net/net_socket_ssl_mbed.c` (vendored)

New entry point `ssl_socket_set_verify_mode(unsigned mode)` called from the RA side before `ssl_socket_connect`. Sets a static module-scope variable that `ssl_socket_connect` reads and translates into the mbedtls authmode.

```c
static int ssl_authmode = MBEDTLS_SSL_VERIFY_REQUIRED;

void ssl_socket_set_verify_mode(unsigned mode)
{
   switch (mode)
   {
      case 1:  ssl_authmode = MBEDTLS_SSL_VERIFY_OPTIONAL; break;
      case 2:  ssl_authmode = MBEDTLS_SSL_VERIFY_NONE;     break;
      default: ssl_authmode = MBEDTLS_SSL_VERIFY_REQUIRED; break;
   }
}

/* in ssl_socket_connect, replacing line 199: */
mbedtls_ssl_conf_authmode(&state->conf, ssl_authmode);

/* and after line 220's existing get_verify_result block, add: */
if (ssl_authmode == MBEDTLS_SSL_VERIFY_REQUIRED && flags != 0)
{
   char vrfy_buf[512];
   mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);
   RARCH_ERR("[TLS] Cert verification failed for %s: %s\n",
         state->domain, vrfy_buf);
   /* mbedtls_ssl_handshake already returned non-zero in this case;
    * fail-closed: return -1 instead of state->net_ctx.fd. */
   return -1;
}
else if (flags != 0)
{
   /* OPTIONAL or DISABLED — warn but allow */
   char vrfy_buf[512];
   mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);
   RARCH_WARN("[TLS] Cert verification soft-failed for %s "
         "(mode=%s): %s\n", state->domain,
         ssl_authmode == MBEDTLS_SSL_VERIFY_NONE ? "DISABLED" : "OPTIONAL",
         vrfy_buf);
}
```

The static module-scope variable is a deliberate choice — the alternative is threading the mode through every `ssl_socket_connect` caller, which means touching ~5 vendored files. The static is set once at startup and on each settings change.

### `network/cloud_sync/*.c`, `cheevos/cheevos_client.c`, `tasks/task_http.c`

No changes — they call `ssl_socket_connect` and the new fail-closed behaviour propagates as a connect failure they already handle.

### `menu/menu_setting.c`

New setting under *Settings → Network → Advanced → TLS Verification*: enum dropdown with three values. On change, calls `ssl_socket_set_verify_mode(new_value)`. On change to *Disabled*, opens the consent dialog described in D3.

### Startup wire-up

In `retroarch_main_init` after settings are loaded, call `ssl_socket_set_verify_mode(settings->uints.tls_verify_mode)` once.

## Test plan

### Manual conformance

1. **Default required (clean install):** `curl -k https://untrusted-root.badssl.com/`-equivalent — point RA at a self-signed test endpoint. Expect: connect fails, `RARCH_ERR` in log, the cloud-sync UI shows "Connection failed."
2. **Optional mode:** flip setting to *optional*. Same self-signed endpoint. Expect: connect succeeds, `RARCH_WARN` in log every time.
3. **Disabled mode:** flip setting to *disabled*. Confirmation dialog appears. Confirm. Same self-signed endpoint. Expect: connect succeeds, `RARCH_WARN("certificate verification disabled — vulnerable to MITM")` on every connect, on-screen toast on first connect of session.
4. **Round-trip:** flip *disabled* → save config → restart RA → check `retroarch.cfg` says `tls_verify_mode = "2"` → check the on-screen toast appears on first connect.
5. **Real cert (good path):** point at `https://retroachievements.org/` (real CA-signed). All three modes succeed. *Required* and *optional* with no warning. *Disabled* with the per-connect warning.

### Automated

- Add a unit test or test fixture (under `tests/` if RA has one) that spins up a Python or Go HTTPS server with a self-signed cert and exercises all three modes.
- Add a regression test that loads a known-good cert and confirms `flags == 0` on the verify result in *required* mode.

### Build verification

`make -j$(nproc) retroarch` clean. Both with and without `HAVE_SSL` to verify the non-TLS build path (which uses plain `socket_*`) is unchanged.

## Risks

- **Vendored fix means resync churn.** Every libretro-common pull will need to re-apply the local fix until upstream merges it. Mitigation: pin the local patch to a specific upstream commit hash in a `LIBRETRO_COMMON_PIN` variable; refuse to resync past that hash without re-testing.
- **Existing users on broken CA bundles see fresh failures.** The bundled `cacert.pem` is updated periodically but not on every release. Users on old-enough builds may have fresh-LetsEncrypt-cert connect failures post-patch. Mitigation: release note explicitly says "if downloads/cloud-sync started failing, try Settings → Network → Advanced → TLS Verification → Optional and report the failing host."
- **Enterprise TLS-intercepting proxies.** Users behind a corporate MITM proxy (Zscaler, Bluecoat, etc.) currently work by accident under VERIFY_OPTIONAL. Post-patch they will fail unless they install their corp root CA into the bundled bundle (no UI for that yet — separate v2 spec) or flip to *optional* / *disabled*. Document the workaround in the release note.
- **Cheevos' own login flow.** Cheevos has its own retry-on-fail semantics. Confirm that a hard connect-failure (REQUIRED mode against a borked cert) doesn't infinite-loop the cheevos retry logic.
- **Upstream PR may be rejected.** If libretro/libretro-common rejects the upstream PR, the local fix lives forever. Mitigation: keep the local patch minimal (one entry point + one switch) so resync conflicts are small.

## External references

- *RFC 5280* — Internet X.509 PKI Certificate and CRL Profile. Defines the validation rules the verify modes implement.
- *mbedtls API* — `mbedtls_ssl_conf_authmode` documentation. `MBEDTLS_SSL_VERIFY_NONE` / `_OPTIONAL` / `_REQUIRED` semantics. (`include/mbedtls/ssl.h` of the vendored copy at `libretro-common/include/`).
- *Mozilla NSS root list* — `cacert.pem` is sourced from this. <https://wiki.mozilla.org/CA/Included_Certificates>.
- *OWASP Mobile Security Testing Guide §M3* — Insecure Communication. The current state is precisely the "MSTG-NETWORK-3 fail" pattern.

## Implementation phases

1. **Phase 1 (0.5 day) — RA-side scaffolding.** Add the enum, the setting, the consent dialog, the `ssl_socket_set_verify_mode` wire-up. No actual mbedtls change yet — the function exists but does nothing. Ship + verify the menu UI works.
2. **Phase 2 (0.5 day) — vendored fix.** Apply the libretro-common edit (the static + the authmode switch + the fail-closed return). Verify all four manual conformance tests.
3. **Phase 3 (0.5 day) — telemetry + rollout signals.** Add the on-screen toast on first-connect-of-session in *disabled* mode. Add a one-time release-note popup on first launch after upgrade explaining the change.
4. **Phase 4 (open-ended) — upstream PR.** Submit to libretro/libretro-common. Pin the RA-side patch.

Phases 1–3 are one bundle (~1.5 days). Phase 4 is concurrent / open-ended.

## Spec status

Draft — awaiting:
1. **D1:** two modes or three? Recommend (B) three.
2. **D2:** UI placement — Network root vs. Advanced. Recommend (B) Advanced.
3. **D3:** persistence of disabled mode + consent flow. Recommend persist + one-time dialog.
4. **D4:** warning surface — log + toast vs. badge. Recommend log + toast, no badge.
5. **Threat-model agreement:** is "MITM on untrusted WiFi" the right framing for the user-facing language, or do we soften it to "untrusted-network exposure"?
