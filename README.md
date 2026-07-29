# Mupen64Plus-Highscore

This is a Mupen64Plus core for Highscore.

Since Mupen64Plus is modular, the core is organized as a mini-frontend rather than a port, with custom
input and audio plugins, and using upstream and unmodified
[mupen64plus-core](https://github.com/mupen64plus/mupen64plus-core/).

It can use the following RSP and video plugins:

## LLE plugins

In LLE mode the core uses [parallel-RSP](https://github.com/highscore-emu/parallel-rsp) and
[parallel-RDP](https://github.com/highscore-emu/parallel-rdp).

Both are based on [RMG](https://github.com/Rosalie241/RMG)'s work for turning them into Mupen plugins,
then further modified to build and install separately rather than as subprojects.

On top of that, the RDP plugin is further modified to stretch the image instead of letterboxing it, so
that we can set the resolution to 640x240 (640x288 for PAL), and then stretch and deinterlace it on
the app side. In particular, this is required for the CRT filter to behave correctly.

The RDP plugin is also ported to C++ and Meson, simply for the sake of convenience.

The core relies on these modifcations to work correctly, and will not work with RMG versions.

# HLE plugins

Alternatively, the core can use [mupen64plus-rsp-hle](https://github.com/mupen64plus/mupen64plus-rsp-hle)
and [GLideN64](https://github.com/gonetz/GLideN64). Both are upstream, unmodified.

---

Which set of plugins to use will be decided by the frontend, presented as an accurate/performance mode
setting and defaulting to accurate (LLE) whenever possible. Whether it's possible is decided by the
frontend too.

Other plugins, like angrylion-rdp-plus, are not supported.
