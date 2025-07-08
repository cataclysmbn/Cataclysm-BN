---
title: CMake builds on Atomic Distros
---

This article goes over building BN on distros that are immutable, and thus where containerization is
the desired solution rather than layering the dependencies onto your base install.

# Example: Fedora Atomic-based (Bazzite)

:::caution

As of writing this, Bazzite's default container image is fedora-toolbox:38, which _may_ result in
having to edit the dependencies installation script. On distros that grab a more modern version of
Fedora as their image (or by manually grabbing one yourself), you can be more certain in just using
the standard Fedora script

:::

:::caution

when using [distrobox](https://distrobox.it), using
[exported](https://github.com/89luca89/distrobox/blob/main/docs/usage/distrobox-export.md) compiler
(e.g `~/.local/bin/clang`)
[won't work as it cannot access `/usr`.](https://github.com/89luca89/distrobox/issues/1548) instead,
use absolute path for compilers like `/usr/bin/clang`.

:::

## Setting up the container

Bazzite, being based on the Atomic versions of Fedora Linux, has the containerization tool known as
Toolbx pre-installed. As such, it is the method we will be using for this example.

To start, create a toolbox with:

```sh
$ toolbox create
```

This will likely prompt you to download an image to base the container on. If this asks about
downloading "fedora-toolbox:38" (or any other number) or asks about downloading Fedora Workstation,
then you're on the right track! Answer 'y'es, and wait for the container to be built. This can take
some time, depending on your hardware, but is fortunately a step you should only need to take once.

After creating your toolbox, navigate to your Cataclysm-BN folder **with a `src` folder.** (I.e.
your local copy of the github repo or your downloaded source code). There, you can enter into your
toolbox with a simple command:

```sh
$ toolbox enter
```

You'll need to run this command every time you intend on building in order to enter into your
container.

Once you're in your toolbox, you can install your dependencies with a Fedora-based distro
installation script. For Bazzite, that script looks like:

```sh
$ sudo dnf install git cmake clang ninja-build mold ccache \
  SDL2-devel SDL2_image-devel SDL2_ttf-devel SDL2_mixer-devel \
  freetype glibc bzip2 zlib-ng libvorbis ncurses gettext flac-devel \
  sqlite-devel zlib-devel
```

After this, your container is set up for all your building needs in the future! Now, onto the steps
of actually building.

## Building

If you aren't already in your container, in the Cataclysm-BN folder, then navigate to the folder and
enter into the container. From there, you may run the cmake script to generate its files (don't
worry about creating the build folder yourself, cmake will automatically generate it if it's not
present already) For Bazzite, this looks like:

```sh
cmake \
  -B build \
  -G Ninja \
  -DCATA_CCACHE=ON \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_INSTALL_PREFIX=$HOME/.local/share \
  -DJSON_FORMAT=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCURSES=OFF \
  -DTILES=ON \
  -DSOUND=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCATA_CLANG_TIDY_PLUGIN=OFF \
  -DLUA=ON \
  -DBACKTRACE=ON \
  -DLINKER=mold \
  -DUSE_XDG_DIR=ON \
  -DUSE_HOME_DIR=OFF \
  -DUSE_PREFIX_DATA_DIR=OFF \
  -DUSE_TRACY=ON \
  -DTRACY_VERSION=master \
  -DTRACY_ON_DEMAND=ON \
  -DTRACY_ONLY_IPV4=ON
```

Assuming all goes well, you should now have your CMake files generated! Now, all you need to do is
run the Ninja command to actually build.

```sh
$ ninja -C build -j $(nproc) -k 0 cataclysm-bn-tiles
```

`$(nproc)` just grabs the number of "threads" your CPU has, but you can specify a lower number if
you'd prefer to use less threads for compiling (albeit at the cost of performance).
