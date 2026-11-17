# 7.24 — sound without a DSP firmware dump: OpenDSP is linked in

- **No `sdmc:/3ds/dspfirm.cdc` and no DSP1 homebrew any more.** The 3DS build now links
  [OpenDSP](https://github.com/Cellenseres/OpenDSP) (BSD-2-Clause), an open-source replacement for the proprietary
  DSP firmware. `audio.cpp` calls `opendspInit()` / `opendspExit()`; the other ndsp calls (24 channels, PCM16, linear
  memory, `DSP_FlushDataCache`) stay as they were. The firmware image and the codec tables are embedded in
  `libopendsp.a`, so nothing extra goes on the SD card.
- **A fallback** keeps older setups working: if `opendspInit()` fails, the client tries libctru's `ndspInit()` and the
  user's own `dspfirm.cdc` as before. The sound status line says which one is in use (`[OpenDSP]` / `[dspfirm]`).
- **Build:** the first `make` fetches the pinned OpenDSP release (`OPENDSP_TAG`, default `v1.3.1`) into
  `proto3ds/extern/OpenDSP/` (git and the usual `DEVKITPRO` environment are needed), builds `library/libopendsp.a` and
  links it before libctru (`-lopendsp -lcitro3d -lctru -lm`). The folder is in `.gitignore`; later builds reuse it.
  `make clean` leaves it alone, and `OPENDSP_TAG=main make` picks another revision.
- **License:** OpenDSP is BSD-2-Clause; its notice is in [`proto3ds/OPENDSP_LICENSE.txt`](../proto3ds/OPENDSP_LICENSE.txt)
  and must stay with binary releases.
- The 24-channel layout is unchanged and fits OpenDSP's 24 voices: music 0, ambience 1, six event channels 2..7 and
  sixteen world channels 8..23.
- Tests: the 3DS build fetches the library, compiles and links (`w3dproto.3dsx`, `w3dproto.cia`); `opendspInit` /
  `opendspExit` are in the ELF. Not tried on hardware yet (New 3DS with Luma3DS is OpenDSP's tested setup; the ndsp
  fallback is there for anything else).
