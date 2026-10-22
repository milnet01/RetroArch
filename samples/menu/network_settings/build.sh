#!/bin/sh
# Builds the menu network settings harness; ../build_shipping_harness.sh says how,
# and what it needs first.
exec "$(dirname "$0")/../build_shipping_harness.sh" network_settings menu_network_settings_test
