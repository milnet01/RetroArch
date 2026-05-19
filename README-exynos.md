# RetroArch Exynos-G2D video driver

> **⚠️ Historical reference (last verified ~2014, ODROID-X2 / Exynos4412 / kernel 3.15.y).** This driver is still wired in (`HAVE_EXYNOS` in `Makefile.common`, source at `gfx/drivers/exynos_gfx.c`) but the setup story below targets long-EOL hardware and kernels. Expect breakage on current kernels; treat the build steps as a starting point, not a recipe.

The Exynos-G2D video driver for RetroArch uses the Exynos DRM layer for presentation and the Exynos G2D block to scale and blit the emulator framebuffer to the screen. The G2D subsystem is a separate functional block on modern Samsung Exynos SoCs (in particular Exynos4412 and Exynos5250) that accelerates various kind of 2D blit operations. It can fill, copy, scale and blend pixel buffers and therefore provides adequate functionality for RetroArch purposes.

## Reasons to use the driver

Hardware accelerated rendering on devices based on an Exynos SoC is usually restricted to the use of the GPU block, which is either a Mali or PowerVR IP. Both GPU types have the problem that interfacing with them requires a proprietary driver stack, comprised of kernel and userspace code. While the kernel code is open source, the userspace code is only available as a binary blob to the enduser.

If you want to use such a device with an upstream kernel, the GPU block will most likely not work for you. Also the chances of Mali or PowerVR kernel code being accepted upstream is very slim. Still, one might want to ask the question if using the GPU block for such trivial operations (basically scale and blend) is the right approach in the first place.

Since the G2D block is present on all modern Exynos SoCs, the natural way of proceeding would be to use it instead of the GPU block. The G2D is still a dedicated piece of hardware, so all operations are offloaded from the CPU. It should be noted though, that using the G2D instead of the GPU removes the possibility to use GPU shaders to enhance the image quality of your emulator core of choice. If the user relies on these enhancements, then he's advised to continue using the GPU, most likely by using the EGL/GLES video driver.

The author uses a Hardkernel ODROID-X2, which is an developer board powered by an Exynos4412 SoC. The vendor supplied kernel, a Linux tree based on the 3.8.y branch, currently offers no way to use the G2D because of issues related to clock setup. However upstreaming work is in progress and a tree based on 3.15.y, with some slight modifications, was historically available as `github.com/tobiasjakobi/linux-odroid`.

> **Note (2026):** the `tobiasjakobi/linux-odroid` fork URL above 404s today and has no working public mirror. The Exynos DRM driver and userspace API landed in mainline Linux and mainline `libdrm` years ago; verify your distro carries it via `pkg-config --modversion libdrm_exynos` before chasing the historical patches. A web-archive snapshot of the fork's README may still be reachable via the Wayback Machine if the original commit history is required.

There is no `README-ODROID` in this repository; an older revision of this file referenced one that has not been tracked in tree.

## Performance analysis

Some simple benchmarking was done to evaluate the performance of the G2D block. The test run was done with the snes9x-next emulation core and a game title that uses a native resolution of 256x224 pixels. The output screen was configured to a 1280x720 mode. Scaling to the output screen was done by keeping the native aspect ratio. In this case this would result in an output rectangle of size 822x720.

    total memcpy calls: 18795
    total g2d calls: 18795
    total memcpy time: 8.978532 seconds
    total g2d time: 29.703944 seconds
    average time per memcpy call: 477.708540 microseconds
    average time per g2d call: 1580.417345 microseconds

The average time to display the emulator framebuffer on screen is roughly 2058 microseconds, or around 486 frames per second. Assuming that the time consumption increases linearly with the amount of pixels processed, which is usually a safe assumption, scaling to an output rectangle of size 1920x1080 would yield a average duration of 7207 microseconds, which is still 138 frames per second.

## Configuration

The video driver uses the libdrm API to interface with the DRM. Historically the user was advised to use the 'exynos' branch of `github.com/tobiasjakobi/libdrm` (URL now 404 — see the note above); the Exynos DRM API has since been merged into mainline libdrm.

If you're building libdrm from source on a current tree, the Exynos API is enabled via:

    ./configure --enable-exynos-experimental-api

(older mainline) or the equivalent `-Dlibdrm-exynos=true` meson option on recent libdrm.

The video driver name is 'exynos'. It honors the following video settings:

   - `video_monitor_index`
   - `video_fullscreen_x` and `video_fullscreen_y`

The monitor index maps to the DRM connector index. If it is zero, then it just selects the first 'sane' connector, which means that it is connected to a display device and it provides at least one useable mode. If the value is non-zero, it forces the selection of this connector. For example, on the author's ODROID-X2, with an odroid-3.15.y kernel, the HDMI connector has index 1.

The two fullscreen parameters select the mode the DRM should select. If zero, the native connector mode is selected. If non-zero, the DRM tries to select the wanted mode. This might fail if the mode is not available from the connector.

## Issues and TODOs

The driver still suffers from some issues.

   - The aspect ratio computation can be improved. In particular the user supplied aspect ratio is currently unused.
   - Font rendering and blitting is very inefficient since the backing buffer is cleared every frame. Introduce a invalidation rectangle which covers the region where font glyphs are drawn, and then only clear this region. Also consider converting the buffer to A8 color format and using global color (not sure if this is possible).
   - Temporary GEM buffers are used as source for blitting operations. Support for the IOMMU has to be enabled, so that one can use the 'userptr' functionality.
   - More TODOs are pointed out in the code itself.
