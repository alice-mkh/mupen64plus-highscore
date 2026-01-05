#!/bin/bash

# A script for building Mupen64Plus-Core and plugins for hacking in toolbox

pushd mupen64plus-core/projects/unix
make all -j 4 \
    PREFIX=/usr \
    OSD=0 \
    NEW_DYNAREC=1
sudo make install \
    PREFIX=/usr \
    LIBDIR=/usr/lib64 \
    OSD=0 \
    NEW_DYNAREC=1
popd

pushd mupen64plus-rsp-hle/projects/unix
make all -j 4 \
    APIDIR=/usr/include/mupen64plus
sudo make install \
    PREFIX=/usr \
    LIBDIR=/usr/lib64 \
    APIDIR=/usr/include/mupen64plus
popd

pushd GLideN64/src
mkdir build
cd build
cmake .. \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_LIBDIR=lib64 \
    -DMUPENPLUSAPI=ON \
    -DNO_OSD=ON \
    -DNOHQ=ON
cmake --build . --parallel
sudo cmake --install .
popd

pushd parallel-rsp
mkdir build
cd build
cmake .. \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build . --parallel
sudo cmake --install .
popd

pushd parallel-rdp
meson setup build --prefix=/usr --optimization=2
meson compile -C build
sudo meson install -C build
popd
