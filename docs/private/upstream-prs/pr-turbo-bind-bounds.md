# PR draft — menu_setting: keep retropad-bind left/right inside input_config_bind_order

Branch `pr/turbo-bind-bounds` in `/mnt/Games/Scripts/Linux/ra-pr`, one
commit on upstream/master `861bd6a089`: `366b950d75` (RETR-0019). Opened
2026-10-02 as libretro/RetroArch#19679.

## Title

menu_setting: keep retropad-bind left/right inside input_config_bind_order

## Body

`input_config_bind_order` has 24 entries. Turbo Bind's range runs to
`RARCH_ANALOG_BIND_LIST_END - 1` (`settings/settings_def_input_turbo_fire.h`),
the last index of that array.

- **Right:** on the last entry, `setting_action_right_retropad_bind`
  reads `input_config_bind_order[i + 1]`, one past the end. With menu
  wraparound off, that value is stored in `input_turbo_bind`. This
  now stops at the last entry; the existing wraparound block still
  handles the wrap.
- **Left:** `setting_action_left_retropad_bind` indexes
  `input_config_bind_order[value]` with the stored value unchecked.
  `input_turbo_bind` is read from the config file without a clamp, so
  a hand-edited value of 24 or more reads past the end. A value above
  the setting's max is now treated like a negative one.

### Testing

Linux: full `make -j4`, no new warnings. Found by reading the code
after a clang-analyzer `security.ArrayBound` report; the over-read was
not observed at runtime.

Made with Claude Code, reviewed and build-tested on our fork.
