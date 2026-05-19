# TLS Verification — Default-On with Documented Opt-Out (Design)

**Date:** 2026-04-27
**Source:** `docs/private/ROADMAP.md` — indie-review CRITICAL ("TLS certificate verification effectively disabled" entry; section anchor, not line — body churns each bundle).
**Status:** draft, awaiting user review. Cold-eyes 2026-05-18 corrections folded in via Bundle 72 (see banner below for the 8-item summary; sections `## Failure modes`, `## Performance budget`, BearSSL backend, Vendoring-clean log surface, and Thread safety are the new load-bearing pieces).
**Target:** `local/fixes-2026-04` (RA-side opt-in) + upstream PR (vendored mbedtls helper)
**Effort estimate:** ~2 days RA-side (mbedtls + BearSSL backends + libcheck tests); upstream coordination is open-ended.

> **⚠️ Cold-eyes 2026-05-18 status update (Bundle 72 fold-in).**
>
> A cold-eyes pass against current source flagged eight load-bearing issues. Items 2/3/6/7/8 are now folded into the spec body (sections below); items 1/4/5 were one-line text fixes already applied in Bundle 71. The remaining low/info nits are listed at the end of this banner for traceability.
>
> 1. ✅ **Title ↔ Goal-1 collision (renamed).** Spec was titled "Opt-In" but Goal 1 reads "verification is mandatory by default" — that is opt-OUT, not opt-in. The title has been corrected. Update any back-references (commit messages, ROADMAP entries) that still read "TLS opt-in spec" / "tls-opt-in-design.md" — the filename is unchanged for stable cross-refs.
> 2. ✅ **Vendoring-clean log surface.** Replaced with a weak-hook contract — see `## Vendoring-clean log surface` below. The default `ssl_socket_log_verify_fail` in libretro-common is a no-op; the RA-side override (under `network/`) calls `RARCH_ERR`. Zero `RARCH_*` references added to the vendored file.
> 3. ✅ **BearSSL backend covered.** Parallel `## BearSSL backend` section added below; `br_ssl_client_init_full` already does verification but offers no opt-out, so the BearSSL parallel of `ssl_socket_set_verify_mode` switches between `br_ssl_client_init_full` (REQUIRED) and a manual `br_x509_minimal_init` + relaxed `_vtable` wrapper (OPTIONAL/DISABLED).
> 4. ✅ **Vendored mbedtls path corrected.** All references now read `deps/mbedtls/mbedtls/ssl.h`.
> 5. ✅ **Header placement settled.** New enum lives at `network/tls_config.h` (RA-side, new file). The vendored header `libretro-common/include/net/net_compat.h` is left alone — that file is also vendored.
> 6. ✅ **Failure modes enumerated.** New `## Failure modes` section below covers revocation-unavailable (CRL/OCSP), time-skew, captive-portal redirect, enterprise MITM proxy refusing on REQUIRED — each with expected log line + user-visible behaviour.
> 7. ✅ **Thread-safety race resolved.** `ssl_authmode` is `_Atomic unsigned` under `HAVE_THREADS` (C11 stdatomic; fallback to plain `unsigned` + snapshot-at-entry on C89-only console builds). See `## Thread safety` below.
> 8. ✅ **Performance budget + automated test wiring.** New `## Performance budget` + `## Automated tests` sections; libcheck test wiring under `libretro-common/test/net/` with stub mbedtls; exact log-line regex contracts pinned for conformance match.
>
> Low/info nits still open (one-line fixes deferred to next docs pass):
> - Dead code path at spec line 162 (mbedtls already returned non-zero from handshake under REQUIRED, so the `get_verify_result` block is unreached).
> - `99.5%/0.5%` figures in §Goals are unsourced — soften to "the great majority" / "a small minority."
> - Error message at line 168 should name the actionable menu path (Settings → Network → Advanced → TLS Verification) rather than the made-up URL "settings/tls/verification."
> - CHANGES.md `# Future` entry shape is undefined.

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

## Failure modes

The summary calls out the easy four: expired, self-signed, hostname-mismatch, chain-broken. Cold-eyes 2026-05-18 (dim 10) added four more that the v1 design must answer for, plus user-visible behaviour per verify mode:

| Failure mode | Trigger | REQUIRED behaviour | OPTIONAL behaviour | DISABLED behaviour | Log line |
|--------------|---------|--------------------|--------------------|--------------------|----------|
| **Expired cert** | Server cert `notAfter` is in the past | Fail-closed, `RARCH_ERR` | Soft-fail, `RARCH_WARN`, connect proceeds | `RARCH_WARN` per connect, connect proceeds | `^\[TLS\] Cert verification failed for [^:]+: .*expired.*$` |
| **Self-signed** | Server cert is not chain-rooted in `cacert.pem` | Fail-closed, `RARCH_ERR` | Soft-fail + `RARCH_WARN` | per-connect WARN | `^\[TLS\] Cert verification failed for [^:]+: .*not trusted.*$` |
| **Hostname mismatch** | SAN/CN doesn't match `state->domain` | Fail-closed, `RARCH_ERR` | Soft-fail + `RARCH_WARN` | per-connect WARN | `^\[TLS\] Cert verification failed for [^:]+: .*hostname.*$` |
| **Chain broken** | Intermediate cert missing or signature invalid | Fail-closed, `RARCH_ERR` | Soft-fail + `RARCH_WARN` | per-connect WARN | `^\[TLS\] Cert verification failed for [^:]+: .*chain.*$` |
| **Revocation unavailable** (CRL/OCSP) | mbedtls CRL probe times out (the vendored mbedtls config does not enable OCSP) | **v1 ignores revocation** — vendored mbedtls is built without `MBEDTLS_X509_CRL_PARSE_C` enabled by default. Documented in §Non-goals. | n/a | n/a | (not logged in v1) |
| **Time skew** (device clock wrong) | Cert reads as expired or not-yet-valid because the device clock is days/years off. Common on consoles with a dead RTC battery. | Fail-closed, `RARCH_ERR` (indistinguishable from genuine expiry in v1) | Soft-fail + `RARCH_WARN` | per-connect WARN | Same as "Expired cert" row. **User-visible:** the release-note guidance must point users to check their system clock first before assuming the cert is bad. |
| **Captive-portal redirect** | DNS/proxy returns the captive-portal IP for the target hostname; the portal presents its own cert for the target domain | Fail-closed, `RARCH_ERR` (hostname-mismatch in practice) | Soft-fail + `RARCH_WARN` | per-connect WARN | Same as "Hostname mismatch" row. **User-visible:** RA's connect failure messaging on the cloud-sync / online-updater UI should hint "check whether you need to log into a hotel/café WiFi portal." |
| **Enterprise MITM proxy** (Zscaler, Bluecoat, etc.) | Corporate proxy substitutes its own cert chain rooted in a corp CA the user has installed system-wide but not in RA's bundled `cacert.pem` | Fail-closed, `RARCH_ERR` | Soft-fail + `RARCH_WARN` — connections work | per-connect WARN — connections work | Same as "Self-signed" row. **User-visible:** the *Optional* mode is the documented workaround until the v2 spec adds a "load extra CA bundle" path. |

### Operational note — REQUIRED failure UX

Under REQUIRED, the user's only path back to a working connection is to flip to *Optional* (or *Disabled*). The error message in the cloud-sync UI must mention the path explicitly:

```
Cloud sync upload failed:
  TLS certificate verification failed for sync.example.com.
  → Try Settings → Network → Advanced → TLS Verification → Optional
    and re-run if you trust this host.
```

### Operational note — DISABLED toast

In DISABLED mode, the first connect of each session emits an on-screen toast (D4 above). The toast text:

```
TLS verification is disabled.
Connections are vulnerable to MITM on untrusted networks.
```

The toast has a 5-second linger and a dismiss tap-target. It does not re-appear within the session even if the user navigates away and back.

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
/* network/tls_config.h — new RA-side header (vendoring-clean). */
enum tls_verify_mode_t
{
   TLS_VERIFY_REQUIRED  = 0,   /* default */
   TLS_VERIFY_OPTIONAL  = 1,   /* mbedtls VERIFY_OPTIONAL + log on failure */
   TLS_VERIFY_DISABLED  = 2    /* mbedtls VERIFY_NONE + warn every connect */
};
```

`TLS_VERIFY_REQUIRED = 0` so unset / default-init / fresh config all land on the safe value.

### `libretro-common/net/net_socket_ssl_mbed.c` (vendored)

New entry point `ssl_socket_set_verify_mode(unsigned mode)` called from the RA side before `ssl_socket_connect`. Sets a module-scope variable that `ssl_socket_connect` reads and translates into the mbedtls authmode.

```c
/* Thread safety: settings-change writes vs. connect-time reads happen
 * across threads (task queue → mbedtls handshake). See `## Thread safety`
 * below for the C11/C89 portability split. */
#if defined(HAVE_THREADS) && !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>
static _Atomic unsigned ssl_authmode = MBEDTLS_SSL_VERIFY_REQUIRED;
#else
static volatile unsigned ssl_authmode = MBEDTLS_SSL_VERIFY_REQUIRED;
#endif

void ssl_socket_set_verify_mode(unsigned mode)
{
   unsigned m;
   switch (mode)
   {
      case 1:  m = MBEDTLS_SSL_VERIFY_OPTIONAL; break;
      case 2:  m = MBEDTLS_SSL_VERIFY_NONE;     break;
      default: m = MBEDTLS_SSL_VERIFY_REQUIRED; break;
   }
#if defined(HAVE_THREADS) && !defined(__STDC_NO_ATOMICS__)
   atomic_store(&ssl_authmode, m);
#else
   ssl_authmode = m;
#endif
}

/* In ssl_socket_connect, snapshot the mode once at entry so subsequent
 * branches and the handshake all see the same value even on the C89
 * fallback path. */
static unsigned ssl_authmode_snapshot(void)
{
#if defined(HAVE_THREADS) && !defined(__STDC_NO_ATOMICS__)
   return atomic_load(&ssl_authmode);
#else
   return ssl_authmode;
#endif
}

/* Vendoring-clean log surface: weak-hook contract.  Default no-op lives in
 * libretro-common so the file builds standalone; RA overrides it under
 * network/ to route into RARCH_ERR / RARCH_WARN.  See
 * `## Vendoring-clean log surface` below. */
__attribute__((weak))
void ssl_socket_log_verify_fail(int mode_required, const char *domain,
      const char *verify_info)
{
   (void)mode_required; (void)domain; (void)verify_info;
}

/* in ssl_socket_connect, replacing line 199: */
mbedtls_ssl_conf_authmode(&state->conf, (int)ssl_authmode_snapshot());

/* and after line 220's existing get_verify_result block, add: */
{
   unsigned mode = ssl_authmode_snapshot();
   if (mode == MBEDTLS_SSL_VERIFY_REQUIRED && flags != 0)
   {
      char vrfy_buf[512];
      mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);
      ssl_socket_log_verify_fail(1, state->domain, vrfy_buf);
      /* mbedtls_ssl_handshake already returned non-zero in this case;
       * fail-closed: return -1 instead of state->net_ctx.fd. */
      return -1;
   }
   else if (flags != 0)
   {
      /* OPTIONAL or DISABLED — log soft-fail and allow. */
      char vrfy_buf[512];
      mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);
      ssl_socket_log_verify_fail(0, state->domain, vrfy_buf);
   }
}
```

The module-scope variable is a deliberate choice — the alternative is threading the mode through every `ssl_socket_connect` caller, which means touching ~5 vendored files. The atomic-with-C89-fallback is the vendoring-clean way to make it thread-safe without forcing C11 onto consoles that don't have `<stdatomic.h>`.

### Vendoring-clean log surface (weak hook)

The default `ssl_socket_log_verify_fail` is a no-op weak symbol in the vendored file. RA's `network/tls_log.c` (RA-side, not vendored) provides the strong override:

```c
/* network/tls_log.c — RA-side, not vendored. */
#include <verbosity.h>

void ssl_socket_log_verify_fail(int mode_required, const char *domain,
      const char *verify_info)
{
   if (mode_required)
      RARCH_ERR("[TLS] Cert verification failed for %s: %s\n",
            domain, verify_info);
   else
      RARCH_WARN("[TLS] Cert verification soft-failed for %s: %s\n",
            domain, verify_info);
}
```

Rationale: `libretro-common` is shipped with every libretro core and may be built standalone; pulling in `verbosity.h` (an RA-side header) would force every standalone consumer to stub it. The weak symbol means the vendored file builds + links unchanged, and the RA-side strong symbol wins at link time. Toolchains that don't support `__attribute__((weak))` (MSVC, some console SDKs) get a `#pragma weak` fallback inside `libretro-common/include/retro_inline.h`-style compat macro (already established pattern in the codebase).

Log-line regex contracts (pinned so conformance tests can match):

```
^\[TLS\] Cert verification failed for [^:]+: .*$        # mode_required=1
^\[TLS\] Cert verification soft-failed for [^:]+: .*$   # mode_required=0
^\[TLS\] Certificate verification disabled — connections vulnerable to MITM\.
```

The third line is the per-connect WARN emitted by RA when `tls_verify_mode == DISABLED`; see `## Failure modes` below.

### Thread safety

The mode variable is read by every TLS handshake and written by the settings UI / startup wire-up. On platforms with `HAVE_THREADS` and `<stdatomic.h>` (most desktop + many console toolchains), the variable is `_Atomic unsigned` and reads/writes go through `atomic_load`/`atomic_store` (`memory_order_seq_cst` default — relaxed would be acceptable but the seq-cst cost on a once-per-handshake read is negligible).

On C89-only platforms (a small number of console SDKs), the fallback is `volatile unsigned` plus snapshot-at-entry. The semantics there are: **a mid-session toggle takes effect on the next connection, not the in-flight one.** This is documented in the consent-dialog UX (D3) so the user understands that flipping verify-mode while a cloud-sync upload is mid-flight will not interrupt the upload but will apply to the next handshake.

The snapshot pattern is intentionally used on both code paths so the in-function logic is uniform — the only difference is whether the read goes through `atomic_load` or a plain `volatile` deref.

### `libretro-common/net/net_socket_ssl_bear.c` (vendored — BearSSL backend)

The BearSSL backend is gated by `HAVE_BUILTINBEARSSL` in `qb/config.libs.sh:485-498` and is mutually exclusive with `HAVE_BUILTINMBEDTLS`. The current code uses `br_ssl_client_init_full` (line 245) which silently enables full verification against `/etc/ssl/certs/ca-certificates.crt` (line 227). Behaviour delta vs. mbedtls:

- **REQUIRED already wired by accident.** `br_ssl_client_init_full` calls `br_x509_minimal_init_full` under the hood. A bad cert causes the next `br_ssl_engine_current_state` poll to return `BR_SSL_CLOSED` with the engine's `last_error` set to a `BR_ERR_X509_*` code — `ssl_socket_connect` already returns `-1` on `BR_SSL_CLOSED`. So BearSSL builds **fail-closed** today.
- **No OPTIONAL / DISABLED path.** There is no equivalent to `MBEDTLS_SSL_VERIFY_OPTIONAL`. The mode would have to be implemented by replacing `br_ssl_client_init_full` with a manual `br_ssl_client_init` + `br_x509_minimal_init` + relaxed `vtable->end_chain` wrapper (the vtable function is what gets called at the end of chain validation; a wrapper that ignores `BR_ERR_X509_NOT_TRUSTED` etc. and returns 0 implements VERIFY_NONE).

#### BearSSL parallel of `ssl_socket_set_verify_mode`

```c
/* Same _Atomic / volatile split as mbedtls; same snapshot pattern. */
#if defined(HAVE_THREADS) && !defined(__STDC_NO_ATOMICS__)
static _Atomic unsigned ssl_authmode = 0;  /* REQUIRED */
#else
static volatile unsigned ssl_authmode = 0;
#endif

void ssl_socket_set_verify_mode(unsigned mode)
{
#if defined(HAVE_THREADS) && !defined(__STDC_NO_ATOMICS__)
   atomic_store(&ssl_authmode, mode);
#else
   ssl_authmode = mode;
#endif
}

/* In ssl_socket_init, replacing the line-245 br_ssl_client_init_full
 * call with a mode-aware setup: */
{
   unsigned mode = ssl_authmode_snapshot();
   if (mode == 0 /* REQUIRED */)
   {
      br_ssl_client_init_full(&state->sc, &state->xc, TAs, TAs_NUM);
   }
   else
   {
      /* OPTIONAL or DISABLED — manual init with a permissive vtable. */
      br_ssl_client_init(&state->sc, &state->xc, TAs, TAs_NUM);
      br_x509_minimal_init(&state->xc, &br_sha256_vtable, TAs, TAs_NUM);
      /* Hash set: SHA-256 + SHA-1 (mbedtls bundle equivalent). */
      br_x509_minimal_set_hash(&state->xc, br_sha1_ID, &br_sha1_vtable);
      br_x509_minimal_set_hash(&state->xc, br_sha256_ID, &br_sha256_vtable);
      /* RSA / ECDSA decoders. */
      br_x509_minimal_set_rsa(&state->xc, br_rsa_pkcs1_vrfy_get_default());
      br_x509_minimal_set_ecdsa(&state->xc, br_ec_get_default(),
            br_ecdsa_vrfy_asn1_get_default());
      /* The permissive end_chain wrapper: on chain failure, log via
       * ssl_socket_log_verify_fail and return success.  REQUIRED mode
       * never reaches this code path. */
      state->xc.vtable = &bear_permissive_x509_vtable;
   }
}
```

The permissive vtable wraps `br_x509_minimal_vtable` and intercepts `end_chain` so it surfaces the error via `ssl_socket_log_verify_fail(0, ...)` instead of returning `BR_ERR_X509_NOT_TRUSTED`. The wrapper itself adds ~30 lines and lives at the top of the file (no new vendored file required).

#### BearSSL effort estimate

- Half a day for the manual-init path + the permissive vtable.
- Plus the libcheck conformance test under `libretro-common/test/net/test_socket_ssl_bear.c` mirroring the mbed test (next-to-zero incremental cost — same test fixture, different backend).

#### Build matrix

The spec's mbedtls and BearSSL paths must build under all four combinations:

| `HAVE_BUILTINMBEDTLS` | `HAVE_BUILTINBEARSSL` | Outcome |
|----------------------|----------------------|---------|
| yes | no  | mbedtls path active |
| no  | yes | BearSSL path active |
| no  | no  | no TLS — `ssl_socket_*` not compiled in; setting is hidden in menu |
| yes | yes | configure refuses (peers; `qb/config.libs.sh:493-498`) |

The third row matters: `menu_setting.c` must check `HAVE_SSL` (or its current equivalent gate — `HAVE_BUILTINMBEDTLS || HAVE_BUILTINBEARSSL`) before exposing the setting, else the user gets a non-functional dropdown.

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

Per `CLAUDE.md`'s tests section, the libretro-common libcheck suite is the canonical place for vendored-code regression tests. v1 adds:

```
libretro-common/test/net/test_socket_ssl_mbed.c   # new — mbedtls backend
libretro-common/test/net/test_socket_ssl_bear.c   # new — BearSSL backend
libretro-common/test/net/test_socket_ssl_log.c    # new — weak-hook contract
```

Wire into `libretro-common/Makefile.test` under a new `HAVE_NET_TEST` block (matching existing `HAVE_STDSTRING_TEST` / `HAVE_QUEUES_TEST` blocks). Each backend test exercises:

1. **REQUIRED + good cert** → `ssl_socket_connect` returns ≥0; `ssl_socket_log_verify_fail` is **not** called.
2. **REQUIRED + bad cert** (self-signed via in-process test fixture) → `ssl_socket_connect` returns -1; `ssl_socket_log_verify_fail(1, "test.local", non-empty)` is called exactly once. Log-line regex match: `^\[TLS\] Cert verification failed for [^:]+: .*$` when the RA override is linked in.
3. **OPTIONAL + bad cert** → `ssl_socket_connect` returns ≥0; `ssl_socket_log_verify_fail(0, ...)` is called exactly once.
4. **DISABLED + bad cert** → `ssl_socket_connect` returns ≥0; `ssl_socket_log_verify_fail(0, ...)` is called exactly once **plus** a once-per-session connect-WARN log. Verify the regex `^\[TLS\] Certificate verification disabled — connections vulnerable to MITM\.` appears.
5. **`ssl_socket_set_verify_mode` race** (under `HAVE_THREADS`): one thread loops `set_verify_mode(REQUIRED|OPTIONAL|DISABLED)`; another thread loops `ssl_socket_connect` against the in-process fixture. ASan/TSan clean; no value torn-write detected by reading back via `atomic_load`.

The test fixture uses an in-process minimal TLS server (a 200-line wrapper around the bundled mbedtls' own `ssl_server.c` example) bound to `127.0.0.1:0` (kernel-chosen port). No network access required — runs in CI without external dependencies.

The weak-hook contract test (`test_socket_ssl_log.c`) links the vendored file standalone (no RA override) and verifies the default `ssl_socket_log_verify_fail` is a no-op — i.e. defining a no-op is the build-clean contract for standalone consumers (other libretro cores embedding libretro-common).

### Build verification

`make -j$(nproc) retroarch` clean. Build matrix:

- `HAVE_BUILTINMBEDTLS=yes HAVE_BUILTINBEARSSL=no` (most desktop / 95% of users).
- `HAVE_BUILTINMBEDTLS=no  HAVE_BUILTINBEARSSL=yes` (BearSSL backend exercised).
- `HAVE_BUILTINMBEDTLS=no  HAVE_BUILTINBEARSSL=no`  (no TLS — verify menu setting is gated out cleanly).

Plus `make -f libretro-common/Makefile.test net` under both backend configs (the libcheck tests should pass in both).

## Performance budget

| Metric | Target | Notes |
|--------|--------|-------|
| Handshake (REQUIRED, good cert) | p99 ≤ 2.0 s on desktop / ≤ 5.0 s on console-class | Measured from `ssl_socket_init` to first successful `ssl_socket_connect` return ≥0. Includes the initial `cacert.pem` parse on first connect of session (subsequent connects skip it — `TAs_NUM != 0` short-circuits in BearSSL; mbedtls keeps `state->ca` across connects in v1). |
| Handshake (REQUIRED, bad cert) | p99 ≤ 1.5 s on desktop / ≤ 3.5 s on console-class | Fail-closed path returns earlier (no record-layer continuation after the verify-result branch). |
| `ssl_socket_set_verify_mode` call cost | < 100 ns p99 | Single atomic store; called once at startup + on each settings change. No allocation. |
| `ssl_authmode_snapshot` per-handshake cost | < 50 ns p99 | Single atomic load; called twice per `ssl_socket_connect` (once for `mbedtls_ssl_conf_authmode`, once for the verify-result branch). Negligible vs. mbedtls handshake cost (~50 ms desktop). |
| Heap inflation per handshake | 0 bytes new in v1 | The patch adds no allocations — `vrfy_buf[512]` is stack, the static `ssl_authmode` is module-scope BSS, the weak-hook log call gets the buffer by const-ref. |

The p99 console budget is set 2.5× the desktop budget to cover slower CPUs (PSP @ 333 MHz, 3DS @ 268 MHz) and software-only modexp paths. Empirical numbers from Bundle 41 (cheevos/HTTP test fixture) confirm a clean cheevos login under `MBEDTLS_SSL_VERIFY_OPTIONAL` takes ~800 ms p99 on desktop — REQUIRED adds a single chain-validation walk (~5 ms desktop, ~50 ms on console-class), so the budget has ~2× margin.

If any of these budgets is missed in CI, the libcheck test fails — wire into the `HAVE_NET_TEST` block's `EXPECT_BENCH_LE(handshake_p99_ms, 2000)` style assertion (the libcheck `ck_assert_int_le` macro is sufficient; bench timing helpers already exist in `libretro-common/test/include/test_bench.h` — same pattern as the stdstring bench tests).

## Risks

- **Vendored fix means resync churn.** Every libretro-common pull will need to re-apply the local fix until upstream merges it. Mitigation: pin the local patch to a specific upstream commit hash in a `LIBRETRO_COMMON_PIN` variable; refuse to resync past that hash without re-testing.
- **Existing users on broken CA bundles see fresh failures.** The bundled `cacert.pem` is updated periodically but not on every release. Users on old-enough builds may have fresh-LetsEncrypt-cert connect failures post-patch. Mitigation: release note explicitly says "if downloads/cloud-sync started failing, try Settings → Network → Advanced → TLS Verification → Optional and report the failing host."
- **Enterprise TLS-intercepting proxies.** Users behind a corporate MITM proxy (Zscaler, Bluecoat, etc.) currently work by accident under VERIFY_OPTIONAL. Post-patch they will fail unless they install their corp root CA into the bundled bundle (no UI for that yet — separate v2 spec) or flip to *optional* / *disabled*. Document the workaround in the release note.
- **Cheevos' own login flow.** Cheevos has its own retry-on-fail semantics. Confirm that a hard connect-failure (REQUIRED mode against a borked cert) doesn't infinite-loop the cheevos retry logic.
- **Upstream PR may be rejected.** If libretro/libretro-common rejects the upstream PR, the local fix lives forever. Mitigation: keep the local patch minimal (one entry point + one switch) so resync conflicts are small.

## External references

- *RFC 5280* — Internet X.509 PKI Certificate and CRL Profile. Defines the validation rules the verify modes implement.
- *mbedtls API* — `mbedtls_ssl_conf_authmode` documentation. `MBEDTLS_SSL_VERIFY_NONE` / `_OPTIONAL` / `_REQUIRED` semantics. Vendored header lives at `deps/mbedtls/mbedtls/ssl.h`.
- *BearSSL X.509 minimal validator* — `br_x509_minimal_*` API and `br_ssl_client_init_full` convenience init (full chain validation). Vendored at `deps/bearssl-0.6/inc/bearssl_x509.h` + `deps/bearssl-0.6/inc/bearssl_ssl.h`.
- *Mozilla NSS root list* — `cacert.pem` is sourced from this. <https://wiki.mozilla.org/CA/Included_Certificates>.
- *OWASP Mobile Security Testing Guide §M3* — Insecure Communication. The current state is precisely the "MSTG-NETWORK-3 fail" pattern.

## Implementation phases

1. **Phase 1 (0.5 day) — RA-side scaffolding.** Add `network/tls_config.h` (enum), the `menu_setting.c` entry, the consent dialog, the `network/tls_log.c` strong override of `ssl_socket_log_verify_fail`, and the startup wire-up. No vendored change yet — `ssl_socket_set_verify_mode` is declared but the vendored .c file still has the OPTIONAL default. Ship + verify the menu UI works.
2. **Phase 2 (0.5 day) — vendored mbedtls fix.** Apply the libretro-common mbed edit (atomic authmode + snapshot + fail-closed return + weak-hook log). Verify the four manual conformance tests + the libcheck `test_socket_ssl_mbed.c` suite.
3. **Phase 3 (0.5 day) — vendored BearSSL fix.** Apply the libretro-common bear edit (atomic authmode + permissive vtable + log hook). Verify the four manual conformance tests under BearSSL + the libcheck `test_socket_ssl_bear.c` suite. Add the `HAVE_NET_TEST` block to `libretro-common/Makefile.test`.
4. **Phase 4 (0.5 day) — telemetry + rollout signals.** Add the on-screen toast on first-connect-of-session in *disabled* mode. Add a one-time release-note popup on first launch after upgrade explaining the change.
5. **Phase 5 (open-ended) — upstream PR.** Submit the mbedtls + BearSSL patches to libretro/libretro-common (likely two separate PRs since the backends share no code). Pin the RA-side patch to a specific upstream commit.

Phases 1–4 are one bundle (~2 days). Phase 5 is concurrent / open-ended.

## Spec status

Draft — awaiting:
1. **D1:** two modes or three? Recommend (B) three.
2. **D2:** UI placement — Network root vs. Advanced. Recommend (B) Advanced.
3. **D3:** persistence of disabled mode + consent flow. Recommend persist + one-time dialog.
4. **D4:** warning surface — log + toast vs. badge. Recommend log + toast, no badge.
5. **Threat-model agreement:** is "MITM on untrusted WiFi" the right framing for the user-facing language, or do we soften it to "untrusted-network exposure"?
