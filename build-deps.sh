#!/bin/bash

# A script to build Mupen64Plus-Core and plugins for hacking in toolbox

git clone https://github.com/mupen64plus/mupen64plus-core
git clone https://github.com/gonetz/GLideN64
git clone https://github.com/alice-mkh/parallel-rdp-standalone
git clone https://github.com/alice-mkh/parallel-rsp
wget https://gitlab.gnome.org/World/highscore/-/raw/main/flatpak/cores/gliden64-framebuffer-fix.patch

pushd mupen64plus-core/projects/unix
make all PREFIX=/usr OSD=0 NEW_DYNAREC=1
sudo make install PREFIX=/usr LIBDIR=/usr/lib64 OSD=0 NEW_DYNAREC=1
popd

pushd GLideN64/src
git am ../../gliden64-framebuffer-fix.patch
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_LIBDIR=lib64 -DMUPENPLUSAPI=ON -DUSE_SYSTEM_LIBS=ON
cmake --build . --parallel
sudo make install
popd

pushd parallel-rsp
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build . --parallel
sudo make install
popd

pushd parallel-rdp-standalone
meson setup build --prefix=/usr --optimization=2
ninja -C build
sudo ninja -C build install
popd
