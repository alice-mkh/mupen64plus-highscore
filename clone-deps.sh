#!/bin/bash

git clone https://github.com/mupen64plus/mupen64plus-core
git clone https://github.com/mupen64plus/mupen64plus-rsp-hle
git clone https://github.com/gonetz/GLideN64
git clone https://github.com/alice-mkh/parallel-rdp-standalone
git clone https://github.com/alice-mkh/parallel-rsp

pushd GLideN64
wget https://gitlab.gnome.org/World/highscore/-/raw/main/flatpak/cores/gliden64-framebuffer-fix.patch
git am gliden64-framebuffer-fix.patch
popd