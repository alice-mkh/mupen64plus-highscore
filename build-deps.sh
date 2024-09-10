#!/bin/bash

# A script to build Mupen64Plus-Core and plugins for hacking in toolbox

git clone https://github.com/mupen64plus/mupen64plus-core
git clone https://github.com/mupen64plus/mupen64plus-rsp-hle
git clone https://github.com/gonetz/GLideN64

pushd mupen64plus-core/projects/unix
make all PREFIX=/usr OSD=0 NEW_DYNAREC=1
sudo make install PREFIX=/usr LIBDIR=/usr/lib64 OSD=0 NEW_DYNAREC=1
popd

pushd mupen64plus-rsp-hle/projects/unix
make all
sudo make install PREFIX=/usr LIBDIR=/usr/lib64
popd

exit 0

pushd GLideN64/src
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo -DMUPENPLUSAPI=1 -DCMAKE_INSTALL_LIBDIR=lib64/highscore/cores
make
sudo make install
popd
