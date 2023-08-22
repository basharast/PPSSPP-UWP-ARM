## Note
Ther will be no further updates here
please follow up at [this link (Discor)](https://discord.com/invite/eYt92NNhNE) or ([GitHub](https://github.com/XboxRetroGaming/PPSSPP-UWP)) for the upcoming update

to help the community to have official release I'm working hard to merge most of the changes in the official repo

until I finish you can enjoy using this unofficial release 

(my main focus is on ARM architect but I will do my best to push a good support for the x64 version).
- [x] Storage solution [PR](https://github.com/hrydgard/ppsspp/pull/17350)
- [ ] Dialogs input
- [ ] Graphic adapters
- [ ] Configs load
- [ ] Post shaders

## Download (old)

<a href="https://github.com/basharast/PPSSPP-UWP-ARM/raw/main/x64/PPSSPP%201.15.3.zip">Click here to download</a>

# Important

This custom release has better performance than the offical release with higher resolutions 4K+

# Changes 

## 1.15.3

- Synced the latest fixes from the official repo
- Disable screen rotation for x64 (maybe it was causing flipped menu issue)

## 1.15.1

- Fixed [free space report](https://github.com/hrydgard/ppsspp/pull/17350/commits/808ff28aa5daf81fcd652c4977d3926409569e9d) and other minor issues

## 1.15.0

- Synced the recent changes from official repo
- More fixes and improvements on the [storage manager](https://github.com/hrydgard/ppsspp/pull/17350/commits/9b0577351fde7ac334bec33ab603cc38b69196dd)

## 1.14.26

- [Storage manager major update](https://github.com/hrydgard/ppsspp/pull/17350/commits/05776ee6af55e162a378a8a384619e4f677ffa8b)

## 1.14.25

- [Minor fixes](https://github.com/hrydgard/ppsspp/pull/17350/commits/cb5d18cb03c6db300bc06027376412d53e783ee0)

## 1.14.24

- Fixed issue with configs were not loaded correctly from custom memory stick
- Fixed issue with custom DPI option at the settings (it was disabled by mistake)
- Many improvements made for UWP storage layer (now it's faster)
- Synced the recent changes from the original source
- It will create Textures, Cheats folders by default

## 1.14.22

- UWP Storage layer
- Custom Memory stick location
- Enabled Browse folder
- Added DPI control (Settings)
- Added support to launch by file or URI (ppsspp:) [LaunchPass, RetroPass supported]
- Dropped RW/RX memory solution, now it works same as desktop
- Enabled Network features like remote disc streaming
