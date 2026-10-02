# PR draft — s3: encode '&', '=' and '?' in the object key

Branch `pr/s3-path-encode` in `/mnt/Games/Scripts/Linux/ra-pr`, one
commit on upstream/master `861bd6a089`: `f33971784b` (RETR-0019). Opened
2026-10-02 as libretro/RetroArch#19680.

## Title

s3: encode '&', '=' and '?' in the object key

## Body

`s3_url_encode` has one caller, `s3_build_request_url`, which passes it
the object key. It leaves `&`, `=` and `?` unencoded as if they were
query delimiters.

- A `?` in a key ends the path where
  `s3_build_canonical_uri_from_url` looks for the query
  (`strpbrk(path_start, "?#")`). The request goes to the wrong key and
  is signed over the wrong URI.
- `&` and `=` are common in save names ("Rocky & Bullwinkle"). S3
  percent-encodes them in its own canonical URI, so the SigV4
  signature most likely does not match and S3 answers 403.

This encodes all three. `/` stays literal.

### Testing

`s3.c` is built only on Apple (`HAVE_S3` in `pkg/apple/BaseConfig.xcconfig`),
so on Linux it was compiled by hand with `-DHAVE_S3 -DHAVE_CLOUDSYNC`,
`-Wall`, no warnings. `s3_url_encode("saves/Rocky & Bullwinkle (USA)=1?.srm")`:

- before: `saves/Rocky%20&%20Bullwinkle%20%28USA%29=1?.srm`
- after: `saves/Rocky%20%26%20Bullwinkle%20%28USA%29%3D1%3F.srm`

Not tested against an S3 server.

Made with Claude Code, reviewed and build-tested on our fork.
