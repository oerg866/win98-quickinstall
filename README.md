# Windows 98 SE QuickInstall

This is the Windows 98 QuickInstall repository.

# Disclaimer

Windows is a trademark that belongs to Microsoft Corporation.

This project has NO ENDORSEMENT FROM ANY INVOLVED PARTIES, SUCH AS MICROSOFT.

Please don't sue. I just like old computers :(

# What does this do?

This is a framework that is intended for creating and preparing Windows 98 installation ISO images in a way that is optimized for extremely quick installation, similar to *nLite*, but with a completely different method and context.

It takes the root file system of an already installed Windows 98 system and packages it, whilst allowing drivers and tools to be slipstreamed at will.

For the installer, it uses Linux as a base, paired with some tools to allow hard disk partitioning and formatting, as well as  a custom installer with a custom data packing method that is optimized for streaming directly from CD to the hard disk without any seeking.

Bottom line, this means that the effort for the user to build an ISO is higher than for example nLite, but the degree of customizability is also massively increased.

## How fast is it really?

On a Pentium III class machine with ATA / ATAPI Ultra DMA available for all storage devices, Windows 98 -- using an ISO built with this framework -- can be installed from CD in roughly 60-90 seconds.

# How to build an ISO image

See [`sysprep/README.txt`](./sysprep/README.txt) for more information.

# Download reference ISO

A reference ISO built using this Package and 98Lite can be downloaded at **archive.org**.

(TODO)

# Building

This was built and tested using Ubuntu 20.04 running natively as well as under **Windows Subsystem For Linux**.

- Install the following packages:

  (TODO, sorry, figure this one out yourself :D)

- Clone this repository
- run init.sh 
- run build.sh
- The `__BIN__` foler will contain the built sysprep environment.

# Special thanks

The gang: kext, ChrisR3tro, Elianda

The many supportive folks at TheRetroWeb

Testing: PhatCJ, agent_x007, limsup, evasive

# License

The `mercypak` and `installer` components have the CC-BY-NC 4.0 license.

https://creativecommons.org/licenses/by-nc/4.0/