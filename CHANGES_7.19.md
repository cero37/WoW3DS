# 7.19 — Charge no longer strands the client in combat, and Feed Pet offers meat and fish

- **Charge.** The client sent the auto-attack (`CMSG_ATTACKSWING`) together with the cast. Charge reaches 8-25 yards, so
  from that distance the server answered `SMSG_ATTACKSWING_NOTINRANGE` and kept no wish of its own. The client's own
  wish stayed on for good, and `inCombat()` counts it: the client then kept the combat stance, the combat controls and
  the Combat tab (the "endless fight"), and only a relog cleared it. Now the spell is remembered instead
  (`attackAfterCast_`) and the swing goes out when the target is inside the server's 5-yard reach (`targetInMelee()`);
  a refused (`SMSG_CAST_FAILED`), interrupted (`SMSG_SPELL_FAILURE`) or unanswered cast drops it. A target that dies,
  goes away or stays out of reach for 8 s drops it too.
- An attack the server refused from out of reach is now retried once a second as soon as the target is in reach
  (`retryAttack()`). The 1.12 server starts melee only on `CMSG_ATTACKSWING` and forgets a refused one, so the swing
  after a charge (and after running up to a target selected from afar) would otherwise never begin.
- **Feed Pet.** Meat and fish are Trade Goods, and the 1.12 server sends their subclass as 0 just as it does for
  consumables, so the food list (it took consumables and Trade Goods/Meat only) left out the very food the PC client
  feeds a pet with. A Trade Good with subclass Meat (8) or with no subclass is offered now as well. The list still
  never guesses the pet's diet: the server answers `SPELL_FAILED_WRONG_PET_FOOD` / `SPELL_FAILED_FOOD_LOWLEVEL` and the
  client prints that line.
- The tapped row now feeds the item that was drawn (`petFeedShown_` holds its guid). Item queries land while the list
  is open and can add rows, so an index alone could feed another item, or nothing.
- Tests: `feed_test` (10 checks) now also expects a Trade Good whose subclass came as 0; `autoattack_test` (16 checks)
  adds Charge: no attack and no combat mode from 20 yd, the swing when the charge lands, and nothing after a refused
  cast. `pet_test`, `selfcast_test`, `cooldown_test`, `useitem_test` and `selfmove_test` still pass (built with the
  Windows llvm-mingw compiler, the same `net/` the 3DS client uses), and `proto3ds` links (`w3dproto.3dsx`, `.cia`).
  Neither fix was tried against a live server yet: the charge needs a warrior with a target beyond melee reach, Feed Pet
  a hunter whose bags hold meat or fish.
