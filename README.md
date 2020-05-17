# pavucontrol-qt

## Overview

pavucontrol-qt is the Qt port of volume control [pavucontrol](https://freedesktop.org/software/pulseaudio/pavucontrol/) of sound server [PulseAudio](https://www.freedesktop.org/wiki/Software/PulseAudio/).   

As such it can be used to adjust all controls provided by PulseAudio as well as some additional settings.   

## Installation

### Compiling source code

Runtime dependencies are qtbase and PulseAudio client library libpulse.   
Additional build dependencies are CMake and optionally Git to pull latest VCS checkouts.

Code configuration is handled by CMake. CMake variable `CMAKE_INSTALL_PREFIX` has to be set to `/usr` on most operating systems.   

To build run `make`, to install `make install` which accepts variable `DESTDIR` as usual.   

### Binary packages

This project was launched in August 2016 and binary packages are rare so far. 

On Arch Linux the package [pavucontrol-qt](https://www.archlinux.org/packages/community/x86_64/pavucontrol-qt/) can be used and [pavucontrol-qt-git](https://aur.archlinux.org/packages/pavucontrol-qt-git/) is to build current checkouts of branch `master`.

On FreeBSD the binary package is available as [pavucontrol-qt](https://www.freshports.org/audio/pavucontrol-qt/) and can be installed with `pkg install pavucontrol-qt`.

## Usage

The usage itself should be self-explanatory.


## Credits

The data/bop.wav is from the sound-theme-freedesktop package.

https://freedesktop.org/wiki/Specifications/sound-theme-spec
