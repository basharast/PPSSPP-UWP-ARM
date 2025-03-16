<p align="center">
  <img src="assets/logo.png" width="176"><br>
  <b>PPSSPP UWP ARM32 only</b><br/>
  <a href="./src">Source</a> |
  <a href="https://github.com/hrydgard/ppsspp">Original Project</a> 
  <br/><br/>
</p>

# Target
Legacy support:
- Build 10586+ 
- DirectX Feature Level 11, 9.3 (~9.1)

## Overview:
[PPSSPP](https://github.com/hrydgard/ppsspp) is PSP emulator by [Henrik Rydgård](https://github.com/hrydgard)

## XBOX (x64)
Please visit [XBOX DevStore](https://xbdev.store/), I host only ARM32 releases

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

Phase 1 ([PR](https://github.com/hrydgard/ppsspp/pull/17350)) [Merged]

Phase 2 ([PR](https://github.com/hrydgard/ppsspp/pull/17952)) [Merged]

- [x] Storage solution
- [x] Dialogs input
- [x] Graphic adapters
- [x] Configs load
- [x] Post shaders

# Support

The whole credits for the good performance goes to Henrik and PPSSPP contributors for keeping the project core compatible with older builds, what I did is a little comparing to the main work

If you found this release helpful and enjoy it, consider supporting the project at the link below:

https://www.ppsspp.org/buygold/

# Build

This more like my test lab, you mostly will not be able to build it for other platforms as I'm not aware to organize things

many stuff used as extern vars for quick and easy test, and I moved some things to another places specificly in 1.15.5.

## Building [1.15.5]

- You need SDKs: 18362, 14393 and 22621 (must)
- Use Visual Studio 2022
- Go to `UWP`
- Open `PPSSPP_UWP.sln` 
- Tests made on `Release` and `UWP Gold`
- Build

## Building [1.17.1]
- Same requirements except it's ready for `10586`
- It has only ARM legacy configuration
- Just select the legacy config and build

## Build Legacy 

To build PPSSPP for `10586`

- You need as extra SDKs: `19041`
- Ensure `CommonUWP` set at `19041` not `22621`
- Ensure `PPSSPP_UWP` set at `19041` not `22621`
- Set `CommonUWP` min target to `10240` or `10586`
- Set `PPSSPP_UWP` min target to `10240` or `10586`
- Add Preprocessors `HTTPS_NOT_AVAILABLE` to `CommonUWP`
- At `CommonUWP` ensure to exclude `ext\naett` from build
- Build using `UWP Gold 14393` configuration only

If you're attempting to make your own from the official source, as extra:
- Ensure `Common/Render/Text/draw_text_uwp.cpp` has same changes I made
- At `Common\Thread\ThreadUtil.cpp` disable `SetThreadDescription(..);`
- Replace anything with `FromApp` with older Win32 API (Except memory functions)
- At `App.cpp` -> `InitialPPSSPP()` ensure `->InstalledPath` replaced by `->InstalledLocation->Path`
- Remove HDMI stuff from `UWP/Common/DeviceResources.cpp`
- Integrating the Storage Layer I made will take much effort on older builds

## Shader Compatibility

For devices such as Lumia 950/950XL & Elite X3 those support DX Feature level 11.x

All other older devices supports only 9.3, surface support 9.1

and this will cause the games not to work at all on older devices,

I don't have good time to point the exact code lines, but here some tips:

- Ensure to remove [assert](https://github.com/hrydgard/ppsspp/blob/832e64b8cd49484a0c44e2c26897f5f7259a3b6a/Common/GPU/D3D11/thin3d_d3d11.cpp#L257) that prevent 9.3 to load
- Remove or disable [vertexIndex](https://github.com/hrydgard/ppsspp/blob/832e64b8cd49484a0c44e2c26897f5f7259a3b6a/Common/GPU/ShaderWriter.cpp#L219) 9.3 don't support that
- Feature level 9.3 don't support bitwise, ensure to fix all shaders do that
- Some shader already has condition for bitwise using `bitwiseOps`
- You can simply force `bitwiseOps` to `false`
- but there are places not covered with this and has no alternative which need to be fixed
- Turn on D3D debug layer and ppsspp debug log and fix other shader problems one by one
- Use multiple files to test, not all games use the same shaders
- I made some changes but they are not complete to make 9.3 work in the same way 11 do

and be aware, if you have device support feature level 11, then don't use 9.3

the modification above will cause slow down, I made them based on my very basic experience

## Conclusion

If this was running with OpenGLES (not ANGLE) would be 2~4 times faster

I don't know much about graphic APIs but from general look DirectX made it worse

specificly with the backward options (feature level 9.1 & 9.3)

I tried a lot, you will have to reduce some processes, ok that doesn't work well if I will play with less visuals

It require a lot of time with good experience to write the shaders and adjust some processes

Get 950 and equal hardware as Android and test to see what I mean (Regardless to the memory restrictions).

In anyway I exposed some shader options to the menu for easy testing (DirectX Shader section).

## Important

This project for legacy support, if something isn't working on your modern hardware

please don't open issue for it, I support only ARM32


# Credits

Henrik Rydgård PPSSPP Emulator

Bashar Astifan (UWP Storage manager and other UWP improvements)
