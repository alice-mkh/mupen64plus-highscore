#!/bin/bash

git clone https://github.com/mupen64plus/mupen64plus-core
git clone https://github.com/mupen64plus/mupen64plus-rsp-hle
git clone https://github.com/gonetz/GLideN64
git clone https://github.com/highscore-emu/parallel-rdp
git clone https://github.com/highscore-emu/parallel-rsp

pushd parallel-rdp
git remote add upstream https://github.com/Themaister/parallel-rdp
git fetch upstream
popd

pushd parallel-rsp
git remote add upstream https://github.com/libretro/parallel-rsp
git fetch upstream
popd
