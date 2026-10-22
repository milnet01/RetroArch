#!/bin/sh
# Builds the Playlists-tab Scan harness; ../build_shipping_harness.sh says how,
# and what it needs first.
exec "$(dirname "$0")/../build_shipping_harness.sh" playlists_scan playlists_scan_test
