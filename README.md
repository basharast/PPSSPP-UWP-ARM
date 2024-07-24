# PPSSPP-UWP (ARM32)
PSP emulator with full-UWP (ARM32) support

Legacy support:
- Build 10586+ 
- DirectX Feature Level 11, 9.3 (~9.1)

## Overview:
[PPSSPP](https://github.com/hrydgard/ppsspp) is PSP emulator by [Henrik Rydgård](https://github.com/hrydgard)

## XBOX (x64)
Please visit [XBOX DevStore](https://xboxdevstore.github.io/), I host only ARM32 releases

the major part of the UWP improvements were merged already into the official repo.

## ARM64 or Latest
- This repo for ARM32, legacy support only
- Refer to the official repo for ARM64 [Click here](https://www.ppsspp.org/download/)
- Please don't ask or contact me for other than ARM32
- This repo is not meant to be up-to date fork (nor redistribution)

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

## Official?

Major part of the UWP improvements already merged in the official repo except for the stuff that related to legacy hardware

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

## Build Legacy

To build PPSSPP for `10586`

- You need as extra SDKs: `19041`
- Ensure `CommonUWP` set at `19041` not `22621`
- Ensure `PPSSPP_UWP` set at `19041` not `22621`
- Set `CommonUWP` min target to `10240` or `10586`
- Set `PPSSPP_UWP` min target to `10240` or `10586`
- Add Preprocessors `HTTPS_NOT_AVAILABLE` & `NO_RAC`
- Build using `UWP Gold 14393` configuration only

## Important

This project for legacy support, if something isn't working on your modern hardware

please don't open issue for it, I support only ARM32


# Credits

Henrik Rydgård PPSSPP Emulator

Bashar Astifan (UWP Storage manager and other UWP improvements)
