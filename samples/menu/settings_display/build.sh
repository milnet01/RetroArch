#!/bin/sh
# Builds the menu settings display harness; ../build_shipping_harness.sh says how,
# and what it needs first.
exec "$(dirname "$0")/../build_shipping_harness.sh" settings_display menu_settings_display_test
