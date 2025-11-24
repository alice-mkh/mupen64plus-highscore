#!/bin/bash

# A script for building Mupen64Plus-Core and plugins for hacking in toolbox

pushd mupen64plus-core/projects/unix
make clean
popd

pushd mupen64plus-rsp-hle/projects/unix
make clean
popd

rm -rf GLideN64/src/build
rm -rf parallel-rsp/build
rm -rf parallel-rdp/build
