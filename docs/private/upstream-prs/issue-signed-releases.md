# Issue draft: sign stable release files

Not posted. Written for RETR-0021 (spec D5 and D8). Post it once the self-updater PR is open, so the request has a user.

---

**Title:** Release signing: publish a signed SHA-256 file for each stable download

**Body:**

**Observation.** Stable downloads have no integrity data beyond the TLS connection that serves them:

- `https://buildbot.libretro.com/stable/1.22.2/windows/x86_64/RetroArch.7z.sha256` answers 404, as do the Win64 installer's and the Linux archive's (checked 2026-10-03 with `curl`).
- The 1.22.2 Win64 installer has no Authenticode signature, and the stable AppImage's `.sha256_sig` and `.sig_key` sections are empty (measured 2026-10-03).

So if the buildbot itself is compromised, a user gets whatever it serves. That matters more once RetroArch can update itself (PR link to follow).

**Proposal.** For each stable file, publish two more files beside it:

- `<file>.sha256`: the output of `sha256sum <file>`.
- `<file>.sha256.sig`: an ECDSA P-256 signature of that `.sha256` file.

The downloads themselves do not change.

**Why P-256.** RetroArch can already check it. `libretro-common/crypto` (`HAVE_CRYPTO`, on by default in `qb/config.params.sh`) has `x509_verify_ecdsa_digest()`. It reads the DER signature `openssl` writes and checks it with `p256_ecdsa_verify()`. Nothing new would be vendored. The tree has no Ed25519 verifier, so Ed25519 would need new code.

**How, for the release manager.**

Once, on an offline machine:

```sh
openssl ecparam -genkey -name prime256v1 -noout -out retroarch-release.pem
openssl ec -in retroarch-release.pem -pubout -out retroarch-release.pub.pem
```

Keep `retroarch-release.pem` offline, with a backup. Commit `retroarch-release.pub.pem` to the repository.

For each release, after the files are built:

```sh
for f in RetroArch.7z RetroArch-Win64-setup.exe RetroArch-Linux-x86_64.AppImage; do
  sha256sum "$f" > "$f.sha256"
  openssl dgst -sha256 -sign retroarch-release.pem -out "$f.sha256.sig" "$f.sha256"
done
```

Anyone can check a download by hand:

```sh
openssl dgst -sha256 -verify retroarch-release.pub.pem \
  -signature RetroArch.7z.sha256.sig RetroArch.7z.sha256 \
  && sha256sum -c RetroArch.7z.sha256
```

**How, in RetroArch.** The public key is built in as its 65-byte uncompressed point. The updater fetches `<file>.sha256` and `<file>.sha256.sig`. It hashes the `.sha256` file with `sha256_stream_*` and checks the signature with `x509_verify_ecdsa_digest()`. A bad signature is a refusal. A good one means the download's own hash must match the `.sha256` file. Once a build carries the key, a missing signature is also a refusal; otherwise an attacker would just delete the `.sig`.

**Tested.** On 2026-10-03 I signed a test file's `.sha256` with OpenSSL 3.5.3, using the commands above. A small program linking only `libretro-common/crypto` and `lrc_hash.c` accepted the signature, and so did `openssl dgst -verify`. The program rejected the same signature over an edited `.sha256`. It also rejected a valid signature made with a different key.

**Optional, and separate.** Authenticode-signing the Windows installer would remove Windows' "unknown publisher" warning. It needs a code-signing certificate bought from a certificate authority. The self-updater does not need it.

This issue was written with the help of Claude Code (an AI assistant), then checked against current upstream code. Happy to adjust anything.
