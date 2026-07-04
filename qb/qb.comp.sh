. qb/config.comp.sh

TEMP_C=.tmp.c
TEMP_CXX=.tmp.cxx
TEMP_EXE=.tmp

CC="${CC:-}"
CXX="${CXX:-}"
PKG_CONF_PATH="${PKG_CONF_PATH:-}"
WINDRES="${WINDRES:-}"

# Checking for working C compiler
cat << EOF > "$TEMP_C"
#include <stdio.h>
int main(void) { puts("Hai world!"); return 0; }
EOF

# test_compiler:
# Check that the compiler exists in the $PATH and works
# $1 = compiler
# $2 = temporary build file
test_compiler ()
{
	compiler=

	for comp in $(printf %s "$1"); do
		if ! next "$comp"; then
			compiler="${compiler} $(exists "${comp}")" ||
				return 1
		fi
	done

	$(printf %s "$1") -o "$TEMP_EXE" "$2" >/dev/null 2>&1 || return 1

	compiler="${compiler# }"
	return 0
}

printf %s 'Checking for suitable working C compiler ... '

cc_works=0
add_opt CC no

if [ "$CC" ]; then
	if test_compiler "$CC" "$TEMP_C"; then
		cc_works=1
	fi
else
	for cc in gcc cc clang; do
		if test_compiler "${CROSS_COMPILE}${cc}" "$TEMP_C"; then
			CC="$compiler"
			cc_works=1
			break
		fi
	done
fi

rm -f -- "$TEMP_C" "$TEMP_EXE"

cc_status='does not work'
if [ "$cc_works" = '1' ]; then
	cc_status='works'
	HAVE_CC='yes'
elif [ -z "$CC" ]; then
	cc_status='not found'
fi

printf %s\\n "$CC $cc_status"

if [ "$cc_works" = '0' ] && [ "$USE_LANG_C" = 'yes' ]; then
	die 1 'Error: Cannot proceed without a working C compiler.'
fi

# Checking whether the mold linker is usable (faster linking).
#   mold is a drop-in, ld-compatible linker (Linux/BSD). Probe by
#   link-testing -fuse-ld=mold with the detected compiler; that link
#   test is the portability guarantee -- it fails cleanly when mold is
#   absent or the (possibly cross) toolchain does not support it, and we
#   fall back to the default linker. --disable-mold skips the probe;
#   --enable-mold warns (non-fatal) if the probe then fails. The flag is
#   appended to LDFLAGS in qb.make.sh, AFTER library detection, so the
#   HAVE_* feature probes are unaffected by the linker choice.
if [ "$cc_works" = '1' ] && [ "$HAVE_MOLD" != 'no' ]; then
	printf %s 'Checking whether the mold linker works ... '

	cat << EOF > "$TEMP_C"
int main(void) { return 0; }
EOF

	if $(printf %s "$CC") -fuse-ld=mold "$TEMP_C" -o "$TEMP_EXE" >/dev/null 2>&1; then
		HAVE_MOLD='yes'
		printf %s\\n 'yes'
	else
		if [ "$USER_MOLD" = 'yes' ]; then
			die : 'Warning: --enable-mold requested but -fuse-ld=mold does not work; using the default linker.'
		fi
		HAVE_MOLD='no'
		printf %s\\n 'no'
	fi

	rm -f -- "$TEMP_C" "$TEMP_EXE"
fi

# Checking for working C++
cat << EOF > "$TEMP_CXX"
#include <iostream>
int main() { std::cout << "Hai guise" << std::endl; return 0; }
EOF

printf %s 'Checking for suitable working C++ compiler ... '

cxx_works=0
add_opt CXX no

if [ "$CXX" ]; then
	if test_compiler "$CXX" "$TEMP_CXX"; then
		cxx_works=1
	fi
else
	for cxx in g++ c++ clang++; do
		if test_compiler "${CROSS_COMPILE}${cxx}" "$TEMP_CXX"; then
			CXX="$compiler"
			cxx_works=1
			break
		fi
	done
fi

rm -f -- "$TEMP_CXX" "$TEMP_EXE"

cxx_status='does not work'
if [ "$cxx_works" = '1' ]; then
	cxx_status='works'
	HAVE_CXX='yes'
elif [ -z "$CXX" ]; then
	cxx_status='not found'
fi

printf %s\\n "$CXX $cxx_status"

if [ "$cxx_works" = '0' ] && [ "$USE_LANG_CXX" = 'yes' ]; then
	die : 'Warning: A working C++ compiler was not found, C++ features will be disabled.'
fi

if [ "$OS" = "Win32" ]; then
	printf %s 'Checking for windres ... '

	WINDRES="${WINDRES:-$(exists "${CROSS_COMPILE}windres" || :)}"

	printf %s\\n "${WINDRES:=none}"

	if [ "$WINDRES" = none ]; then
		die 1 'Error: Cannot proceed without windres.'
	fi
fi

printf %s 'Checking for pkg-config ... '

PKG_CONF_PATH="${PKG_CONF_PATH:-$(exists "${CROSS_COMPILE}pkgconf" ||
	exists "${CROSS_COMPILE}pkg-config" || :)}"

printf %s\\n "${PKG_CONF_PATH:=none}"

if [ "$PKG_CONF_PATH" = "none" ]; then
	die : 'Warning: pkg-config not found, package checks will fail.'
fi
