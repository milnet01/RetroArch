# Mali fbdev OpenGL ES context (NOT a top-level video driver)

> **⚠️ Historical reference.** Targets Allwinner sunxi-3.4-era kernels and r4p0
> Mali userland blobs that are no longer obtainable from the linked sources
> (the ODROID forum thread and `r4p0-mp400-fbdev.tar` URL below both 404 as of
> 2026; the ARM Mali developer portal moved to
> [developer.arm.com](https://developer.arm.com) years ago). The driver itself
> is still wired in (`HAVE_MALI_FBDEV` in `Makefile.common`, source at
> `gfx/drivers_context/mali_fbdev_ctx.c`) but the setup steps target EOL
> hardware. Treat as a porting reference, not a recipe.
>
> **Genre note:** `mali_fbdev` is a *context* driver feeding `gl` / `glcore` on
> fbdev — set `video_driver = gl` (NOT `video_driver = mali_fbdev`) and the
> Mali fbdev context auto-selects when available.

This driver is meant for devices with Allwinner SoCs with a Mali400 3D block
and a good fbdev implementation. It is derived from the old Android GLES
driver.

It was intended for use on the Cubieboard / Cubieboard2 / Cubietruck, but it
should not be used on an ODROID X2 / U2 / U3 where a superior solution
(the RetroArch [Exynos video driver](README-exynos.md)) is available. The
fbdev implementation on ODROID hardware is missing the `WAITFORVSYNC` ioctl,
so the Exynos driver is the right choice there.

This driver requires Mali r4p0 binary blobs for fbdev, and a kernel compatible
with r4p0 binaries.

## Required sources (historical)

The original setup leaned on three sources, all 404 today (kept here as
breadcrumbs for anyone reviving the driver on equivalent hardware):

- Kernel: `github.com/mireq/linux-sunxi` — original sunxi-3.4 fork.
- VSync ioctl patch: `gist.github.com/ssvb/8088519` — small `Fb_wait_for_vsync` enabler.
- Mali r4p0 fbdev blobs: `forum.odroid.com/viewtopic.php?f=52&t=4956` and the direct binary at `builder.mdrjr.net/tools/r4p0-mp400-fbdev.tar`.

If you are reviving this on current hardware, a Wayback Machine snapshot of
the forum thread will typically still resolve via
[web.archive.org](https://web.archive.org/) and is the most reliable way to
recover the blob layout.

## Kernel build

Clone and build the historical sunxi-3.4 kernel:

```sh
git clone https://github.com/mireq/linux-sunxi.git -b sunxi-3.4 --depth 1
```

Edit `drivers/video/sunxi/disp/dev_fb.c` and uncomment line 1074:

```c
// Fb_wait_for_vsync(info);
```

It is assumed you have a cross-compiler installed; configure and build the
kernel and modules:

```sh
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- sun7i_defconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- menuconfig   # optional
make -j4 ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- uImage modules
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \
    INSTALL_MOD_PATH=<path_to_rootfs_mountpoint> modules_install

cp arch/arm/boot/uImage /<path_to_sd_rootfs_mountpoint>/boot/
```

The default config is `sun7i_defconfig` for the Cubieboard2. Other sunxi
boards have their own defconfigs — see the [linux-sunxi wiki kernel page](https://linux-sunxi.org/Linux_Kernel#Compilation)
(still maintained at time of writing).

## Userland blobs and headers

Download and extract the EGL / GLES / GLES2 Mali fbdev blobs from the ODROID
forum thread above. The exact tarball is `r4p0-mp400-fbdev.tar`. Copy the
shared libraries to `/usr/lib`.

Headers historically came from the ARM Mali developer portal's OpenGL ES SDK
for Linux. Only the headers are needed — the rest of the SDK is machine
dependent. Extract the SDK and copy the directories inside `inc/` to
`/usr/include`, then copy `simple_framework/inc/mali/EGL/fbdev_window.h` to
`/usr/include/EGL/`.

After installation the relevant header layout looks like:

```
/usr/include/EGL/
   eglext.h
   egl.h
   eglplatform.h
   fbdev_window.h
/usr/include/GLES/
   glext.h
   gl.h
   glplatform.h
/usr/include/GLES2/
   gl2ext.h
   gl2.h
   gl2platform.h
/usr/include/GLES3/
   gl3ext.h
   gl3.h
   gl3platform.h
```

## Configuring RetroArch

Enable `mali_fbdev` by passing `--enable-opengles --enable-mali_fbdev` to
`configure`. A lean Cubieboard2 build example:

```sh
./configure \
    --enable-opengles --enable-mali_fbdev \
    --disable-x11 --disable-sdl2 --disable-sdl \
    --enable-floathard --enable-udev \
    --disable-ffmpeg --disable-netplay \
    --disable-pulse --disable-oss --disable-freetype --disable-7zip
```

> **Note:** A TTY hack is used to auto-clean the console on exit, and the
> `fbdev` ioctls are used to retrieve the current video mode. Both work, but
> neither is an ideal solution. If you come up with something better, feel
> free to improve the driver.
