# PPSSPP-UWP (ARM)
PSP emulator with full UWP (ARM) support

Legacy support with min build 14393 and D3D Feature Level 9.3

## Overview:
[PPSSPP](https://github.com/hrydgard/ppsspp) is PSP emulator by [Henrik Rydgård](https://github.com/hrydgard)

## UWP Support
Supported by **[UWP2Win32](https://github.com/basharast/UWP2Win32)**

## What's new?

- Choose new Memory stick location (anywhere)
- Browse folder without any problem
- Navigate between folders internally
- Install homebrew (zip) from anywhere
- Use and type into text fields (even on touch devices)
- Use network features like remote play
- Start games by launching the file directly

## XBOX (x64)
Please visit [XBOX DevStore](https://xboxdevstore.github.io/), I host only ARM releases

the major part of the UWP improvements were merged already into the official repo.

## Surface (ARM64)
- Please note Qualcomm and Windows are no longer support ARM32 (which what I do)
- Good news is the ARM64 is officially adopted [Click here](https://www.ppsspp.org/download/)
- My work is focus on legacy support for older devices and ARM devices that still support ARM32

## Roadmap
My main focus is on ARM architect but I will do my best to push a good support for the x64 version

To help the community to have official release I'm working hard to merge most of the changes in the official repo

Phase 1 ([PR](https://github.com/hrydgard/ppsspp/pull/17350)) [Mereged]

Phase 2 ([PR](https://github.com/hrydgard/ppsspp/pull/17952)) [Mereged]

- [x] Storage solution
- [x] Dialogs input
- [x] Graphic adapters
- [x] Configs load
- [x] Post shaders


# Building

- You need SDKs: 18362, 14393 and 22621 (must)
- Use Visual Studio 2022
- Go to `UWP`
- Open `PPSSPP_UWP.sln` 
- Tests made on `Release` and `UWP Gold`
- Build

## Important

This project for legacy support, if something isn't working on your modern hardware

please don't open issue for it, I support only ARM32


# Credits

Henrik Rydgård PPSSPP Emulator

Bashar Astifan (UWP Storage manager and other UWP improvements)
