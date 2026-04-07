# QtVideoPlugins (OES/EGL Zero-Copy)

### 1\. Description

**QtVideoPlugins** is a collection of hardware-accelerated Qt Multimedia backend plugins designed for high-performance, zero-copy video rendering. By leveraging **OpenGL ES (GLES)** and **EGL** extensions, these plugins facilitate efficient buffer sharing (via `dmabuf`) between GStreamer and the GPU, bypassing expensive CPU-side memory copies.

### 2\. Driver & Platform Support

The plugin is designed to be hardware-agnostic and supports:

  * **MESA Open Source Drivers:** Full compatibility with `panfrost`, `vc4`, and `v3d` stacks.
  * **ARM Proprietary Drivers:** Support for vendor-specific EGL implementations.
  * **Display Backends:** \* **KMS/DRM (eglfs):** Ideal for full-screen embedded applications.
      * **Wayland:** Support for modern windowed desktop and embedded environments.
      * **X11:** Support for legacy and standard Linux desktop environments.
  * **ARM/ARM64:** Compatible with the **GStreamer V4L2** and **GStreamer Rockchip MPP** plugins
  * **x86/AMD64:** Compatible with the **GStreamer VA-API** plugin.

### 3\. Implementation Details

The plugin provides two distinct rendering paths depending on the input video format and hardware capabilities:

#### a) OES (Singe-Texture)
* **Extension:** Uses the `EXT_image_dma_buf_import` EGL extension.
* **Mechanism:** Direct import of hardware `dmabuf` memory into a single `GL_TEXTURE_EXTERNAL_OES` texture.
* **Benefit:** Maximum efficiency with zero CPU-to-GPU copies, ideal for NV12 and other hardware-native formats.

#### b) Planar (Multi-Texture)
* **Extension:** Standard `GL_TEXTURE_2D`.
* **Mechanism:** Currently supports **Y12** and **Y21** formats. It creates three separate `GL_TEXTURE_2D` textures (one for each plane) and performs the YUV-to-RGB conversion via a fragment shader.
* **Benefit:** Provides high-performance rendering for 12-bit and 10-bit planar formats on hardware where a single OES import for these specific formats is not supported.

### 4\. Performance Impact

  * **Efficiency:** Performance can improve **2–3 times** depending on the platform due to the elimination of memory bandwidth bottlenecks.
  * **Rockchip RK3568 (Radxa Rock 3B):** Successfully tested playing **4K @ 60fps** video for hardware-accelerated formats (H.264/H.265).
  * **Raspberry Pi 3:** Significant reduction in CPU load during 1080p playback compared to standard raster-based rendering.
  * **x86/AMD64:** While the zero-copy performance gap is smaller on x86, it effectively offloads the rendering pipeline to the GPU, reducing CPU utilization by up to 40% during high-resolution video playback.

-----

### 5\. Cross-Build Instructions (Debian/Ubuntu)

To build this package for an `arm64` target on a Debian/Ubuntu host, follow these steps.

#### Prerequisites

The build process automatically fetches required Qt5 development packages.

```bash
# Install required packages 
sudo apt update
sudo apt install sbuild schroot binfmt-support qemu-user-static debhelper devscripts dh-make

# Setup target root filesystem, choose the one you need instead 'bullseye'
sudo sbuild-createchroot --arch=arm64 bullseye /srv/chroot/bullseye-arm64-sbuild http://deb.debian.org/debian/

# Add user to group sbuild, re-logging is required to make the changes take effect,
sudo adduser $USER sbuild

# Clone the repository
git clone https://github.com/kosvik/QtVideoPlugins.git
cd QtVideoPlugins/oes

# Create original source tarball
tar czf ../qt-video-oes-plugins_1.0.0.orig.tar.gz --exclude=debian .

# Update the changelog to your target version in needed
dch -v 1.0.0 "Initial release for bullseye"
dch -r bullseye

# Build using sbuild for arm64
sbuild --host=arm64 --build=arm64 -d bullseye --chroot bullseye-arm64-sbuild
```

### Native Build Dependencies

The following packages are required in your build environment if you want build on host using qmake:

  * `qtmultimedia5-dev`
  * `qtdeclarative5-dev`
  * `qtdeclarative5-private-dev`
  * `qtbase5-private-dev`
  * `libgstreamer1.0-dev`
  * `libgstreamer-plugins-base1.0-dev`
  * `libdrm-dev`
  * `libasound2-dev`
  * `libpulse-dev`
  * `libegl1-mesa-dev`

-----

### 6\. Yocto Recipe Example

If you are using the Yocto Project / OpenEmbedded, you can use the following recipe logic in a `.bb` file (e.g., `qt-video-oes-plugins_git.bb`):

```bitbake
SUMMARY = "Qt5 Multimedia OES/EGL Video Plugins"
DESCRIPTION = "Hardware-accelerated Qt Multimedia plugins enabling \
 zero-copy video rendering. Utilizes OpenGL ES and EGL extensions \
 for efficient buffer sharing between GStreamer and the GPU."
LIC_FILES_CHKSUM = "file://${WORKDIR}/git/LICENSE;md5=1ebbd3e34237af26da5dc08a4e440464"
LICENSE = "GPL-3.0-only"

# No information for SRC_URI yet (only an external source tree was specified)
SRC_URI = "git://github.com/kosvik/QtVideoPlugins.git;protocol=https;branch=master"

SRCREV = "${AUTOREV}"

PV = "1.0+git"
S = "${WORKDIR}/git/oes"

DEPENDS = "qtmultimedia libdrm gstreamer1.0 virtual/libgles2 virtual/egl"

FILES:${PN} = " \
    ${OE_QMAKE_PATH_PLUGINS}/video/gstvideorenderer \
    ${OE_QMAKE_PATH_PLUGINS}/video/gstvideorenderer/*.so \
    ${OE_QMAKE_PATH_PLUGINS}/video/videonode \
    ${OE_QMAKE_PATH_PLUGINS}/video/videonode/*.so \
"

inherit qmake5 pkgconfig

```

-----

### 7\. License

This project is licensed under the **GPL-3.0-only** License - see the [LICENSE](LICENSE) file for details.
