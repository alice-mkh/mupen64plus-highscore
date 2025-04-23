#!/bin/bash

# A script for pulling Mupen64Plus-Core and plugin updates

pushd mupen64plus-core
git pull origin master
popd

pushd mupen64plus-rsp-hle
git pull origin master
popd

pushd GLideN64
git reset --hard origin/master
git pull origin master
git am gliden64-framebuffer-fix.patch
popd

pushd parallel-rsp
git checkout master
git pull upstream master
git checkout highscore
git merge master
popd

pushd parallel-rdp
git checkout master
git pull upstream master
git checkout highscore
git merge master
popd
