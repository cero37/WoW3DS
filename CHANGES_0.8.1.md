# WoW3DS 0.8.1 — release notes (13 November 2026)

This is 0.8.0 plus the client fixes of the 7.19 and 7.20 sessions (Charge, Feed Pet). Nothing changed in the converter's
data: the packs of 0.8.0 stay valid, and an 0.8.0 client reads an 0.8.1 pack and the other way round. The Windows
converter is the same 0.8.0 build, now made for Windows 7 SP1 and newer (see *Converter* below).

## Charge no longer strands the client in combat (7.19)

- The client used to send the auto-attack (`CMSG_ATTACKSWING`) together with the Charge cast. Charge reaches 8-25 yards,
  so from there the server answered `SMSG_ATTACKSWING_NOTINRANGE` and kept no wish of its own, while the client's own
  wish to attack stayed on for good. `inCombat()` counts that wish: the client then kept the combat stance, the combat
  controls and the Combat tab (the "endless fight"), and only a relog cleared it.
- The spell is remembered now (`attackAfterCast_`) and the swing goes out when the target is inside the server's 5-yard
  reach (`targetInMelee()`). A refused (`SMSG_CAST_FAILED`), interrupted (`SMSG_SPELL_FAILURE`) or unanswered cast drops
  it, and so do a dead or gone target and a target that stays out of reach for 8 s.
- An attack the server refused from out of reach is retried once a second as soon as the target is in reach
  (`retryAttack()`). The 1.12 server starts melee only on `CMSG_ATTACKSWING` and forgets a refused one, so the swing after
  a charge (and after running up to a target picked from afar) would otherwise never begin.

## The end of the charge is confirmed to the server (7.20)

- vMaNGOS starts the charge spline on the player and then ignores **every** movement packet until `CMSG_MOVE_SPLINE_DONE`
  arrives with the spline id and the final position (`HandleMovementOpcodes`: `HasPendingSplineDone()`). The client never
  sent it, so the charge looked right on the screen while the server still had the player where the spline began: blows
  did not land (the range was measured from there), the mobs around the new place never came in, and a relog put the
  character back at the start.
- The spline ids are kept now (`WorldObject::pathSplineId`, from `SMSG_MONSTER_MOVE` and the movement block), and
  `endSelfServerMove()` sends the ack at the end of a non-taxi spline: movement info, the id, and the float the 1.12
  client appends. Taxis are left to the server, which ends them itself.

## Feed Pet offers meat and fish (7.19)

- Meat and fish are Trade Goods, and a 1.12 server may send their subclass as 0 just as it does for consumables. The food
  list took consumables and Trade Goods/Meat (subclass 8) only, so that food could be missing. A Trade Good with subclass
  Meat or with no subclass is offered now as well. The client still never guesses the pet's diet: the server answers
  `SPELL_FAILED_WRONG_PET_FOOD` / `SPELL_FAILED_FOOD_LOWLEVEL` and the client prints that line.
- The tapped row now feeds the item that was drawn (`petFeedShown_` holds its guid). Item queries land while the list is
  open and can add rows, so an index alone could feed another item, or nothing.

## Converter

- `w3dconv`, `w3dgui`, `w3docc` and `w3dpreview` are rebuilt from this tree with llvm-mingw (msvcrt, x86_64, static
  runtime): only system DLLs that Windows 7 carries, no VC++ Redistributable and no UCRT update.
- The previous pack was built with clang-cl and the static runtime of Visual Studio 2026 (MSVC 14.50): its `std::filesystem`
  imports `CreateFile2` from `KERNEL32.dll`, which Windows 7 does not have, so `w3dconv.exe` refused to start there
  ("entry point CreateFile2 was not found"). Nothing else changed: the same 0.8.0 code, the same bats, skin and settings.
- `README_RU.txt` keeps the per-build notes.

## Tests

- `feed_test`: 10 checks (a Trade Good whose subclass came as 0 is offered; ore is not).
- `autoattack_test`: 16 checks; adds Charge: no attack and no combat mode from 20 yd, the swing when the charge lands, and
  nothing after a refused cast.
- `pet_test`, `selfcast_test`, `cooldown_test`, `useitem_test`, `selfmove_test`: pass (Windows llvm-mingw build of `net/`,
  the same code the 3DS client uses). `selfmove_test` (13 checks) now also verifies `CMSG_MOVE_SPLINE_DONE`: the spline's
  end, its id and the 36-byte body, and that a taxi spline sends nothing.
- `proto3ds` builds and links: `w3dproto0.8.1.cia` (178 MB memory mode), `w3dproto0.8.1-124.cia` (124 MB mode) and
  `w3dproto0.8.1.3dsx`; both CIAs are made with `CIA_VERSION=0 8 1` (`-major 0 -minor 8 -micro 1`).
- The converter's imports were checked: `KERNEL32`, `msvcrt`, `WS2_32`, `WININET`, `USER32` only; no `CreateFile2`,
  `CopyFile2` or other Windows 8+ entry points; subsystem 6.0.

Charge was tried on a live server: the first 0.8.1 build charged but could not attack afterwards, and the 7.20 spline ack
is the fix (not yet tried again). Feed Pet was not tried yet (it needs a hunter whose bags hold meat or fish), and neither
was the converter on a real Windows 7 machine.

## Files

| File | What |
|---|---|
| `w3dproto0.8.1.cia` | the client, 178 MB memory mode (New 3DS / New 2DS XL) |
| `w3dproto0.8.1-124.cia` | the 124 MB memory mode (normal New 3DS mode, own title ID: installs next to the 178 MB one) |
| `w3dproto0.8.1.3dsx` | Homebrew Launcher build (124 MB at most) |
| `Convert_pack.0.8.1.zip` | the Windows converter (`w3dconv`, `w3dgui`, `w3docc`, `w3dpreview`, bats), Windows 7 SP1+ |
| `questdb.bin` | the quest database (unchanged from 0.8.0) |
| `CHANGES_0.8.1.md` | these notes |
| `SHA256SUMS_0.8.1.txt` | SHA-256 of the files above |
