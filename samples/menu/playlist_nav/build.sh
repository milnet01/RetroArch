#!/bin/sh
# Builds the menu playlist navigation harness; ../build_shipping_harness.sh says how,
# and what it needs first.
exec "$(dirname "$0")/../build_shipping_harness.sh" playlist_nav menu_playlist_nav_test
