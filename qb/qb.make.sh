# Prefer the mold linker when qb.comp.sh found it usable. Added here,
# after all library detection, so feature probes linked with the default
# linker and only the final build switches to mold. Skipped if LDFLAGS
# already names an explicit linker (respect a user override); an explicit
# NEED_GOLD_LINKER=1 at make time still wins, being appended later in the
# top-level Makefile.
if [ "${HAVE_MOLD:-}" = 'yes' ]; then
	case " $LDFLAGS " in
		*' -fuse-ld='*) : ;;
		*) LDFLAGS="${LDFLAGS} -fuse-ld=mold" ;;
	esac
fi

# Creates config.mk and config.h.
vars=''
add_define MAKEFILE GLOBAL_CONFIG_DIR "$GLOBAL_CONFIG_DIR"
eval "set -- $CONFIG_OPTS"
while [ $# -gt 0 ]; do
	tmpvar="${1%=*}"
	shift 1
	var="${tmpvar#HAVE_}"
	vars="${vars} $var"
done
VARS="$(printf %s "$vars" | tr ' ' '\n' | $SORT)"
create_config_make config.mk $(printf %s "$VARS")
create_config_header config.h $(printf %s "$VARS")
