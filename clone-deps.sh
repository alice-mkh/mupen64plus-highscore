#!/bin/bash

git clone https://github.com/mupen64plus/mupen64plus-core
git clone https://github.com/mupen64plus/mupen64plus-rsp-hle
git clone https://github.com/gonetz/GLideN64
git clone https://github.com/alice-mkh/parallel-rdp
git clone https://github.com/alice-mkh/parallel-rsp

pushd GLideN64
wget https://gitlab.gnome.org/World/highscore/-/raw/main/flatpak/cores/gliden64-framebuffer-fix.patch
wget https://gitlab.gnome.org/World/highscore/-/raw/main/flatpak/cores/gliden64-resize-fix.patch
git am gliden64-framebuffer-fix.patch
git am gliden64-resize-fix.patch
popd

pushd parallel-rdp
git remote add upstream https://github.com/Themaister/parallel-rdp
git fetch upstream
popd

pushd parallel-rsp
git remote add upstream https://github.com/libretro/parallel-rsp
git fetch upstream
popd
