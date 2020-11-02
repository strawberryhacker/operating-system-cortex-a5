<p align="center">
  <img width="460" height="500" src="https://github.com/strawberryhacker/citrusOS/blob/master/doc/cover.png">
</p>

## Summary

Cinnamon is a bare metal lightweight operation system designed for the Cortex®-A series. Every thing will be completely bare metal - no libraries - literally.

## Building

Some packages is required in order to build the operating system. I recomend using Ubuntu, either native or in WSL. The build-essentials package is used for running make, and the gcc-arm-none-eabi is the compiler toolchain used for building and debugging the operating system.

```
> sudo apt update
> sudo apt install build-essential
> sudo apt install gcc-arm-none-eabi
```

Then you must install the second stage bootloader; c-boot, which will load the operating system into main memory.

```
> git clone https://github.com/strawberryhacker/c-boot
> cd c-boot
> make
```

This will generate a binary called **boot.bin** in the build/ folder. Place this file in the **root** directory of a FAT formatted SD card and plug it into the board. The next step is to download and build the kernel. 

```
> git clone https://github.com/strawberryhacker/citrus
> cd citrus
> make CONFIG=sama5d2x_config upload
```

This will load the kernel image to main memory over serial and start it. Support for loading the kernel from an SD card will be added to c-boot later. 
