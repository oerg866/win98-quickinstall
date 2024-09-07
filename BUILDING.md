# Build Process Summary

Here is a summary of the build process:

- Construct root file system
- Compile Kernel in two variants
  - bzImage.cd

    Extended Kernel for CD-ROM boot and 2.88MB floppy disks (if you have such a thing...).

    This version supports USB mass storage drives and is compressed using LZO, which is quite fast to decompress.

  - bzImage.flp

    Minimal Kernel for 1.44MB floppy disks.
    
    This version lacks USB support and is compressed using XZ so it can fit, which decompresses a *lot* slower especially on slow machines. I sadly do not have a way around this at the current time.
    
    This version lacks boot messages in order to save binary size. It is quiet and may seem frozen until it is fully booted.

- Build `dosfstools`
- Build `syslinux`
- Build `util-linux`
- Build `tiny-floppy-bootloader`
- Build `installer`
- Build `mercypak`
- Build CDROM Root Directory in framework directory (`__OUT__`)
- Copy `tools`, `mercypak`, `sysprep`, `win98-driver-lib-base`, `win98-driver-lib-extra` to framework directory
- Zip the framework directory

# BUGS / Limitations

- Linux does not really have a concept of hidden files. It can be faked apparently with unused system attributes, but it might not work properly when copying the files out of the image. Unfortunately, at this time I don't have any solution for this.
- Poor error handling

# Building

## Requirements

The framework must be built on Linux. It was tested using Ubuntu 20.04 and 22.04 running natively as well as under **Windows Subsystem For Linux**.

You must install the following packages:

`build-essential tic python3 python-is-python3 gunzip libuuid wine dd nasm uuid-dev mkisofs zip syslinux automake libtool gettext bison autopoint pkg-config flex`

(this might be incomplete, be warned)

- Clone this repository
- run init.sh 
- run build.sh
- The `__BIN__` foler will contain the built sysprep environment.

# Special thanks

Many people, but especially:

* TheRetroWeb staff: Rigo (0xCats), Computerguy096, Deksor, evasive, agent_x007
* linearcannon, Enigma, limsup, einstein95, Retro Wizard Manfred

# License

The `mercypak` and `installer` components have the CC-BY-NC 4.0 license.

https://creativecommons.org/licenses/by-nc/4.0/
