# 7.23 — a zeppelin crossing to the other continent keeps the player on board

- **The bug.** `SMSG_NEW_WORLD` was always taken as a world position. On a transport the 1.12 server sends the **offset on
  the transport** instead (`vMaNGOS Player::SendNewWorld`: `m_movementInfo.GetTransportPos().WithMapId(...)`), because
  the client is expected to put that offset on the transport object in the new map. The client put the tiny local
  coordinates into the world, cleared the ride (`ride.guid = 0`) and sent movement from there: the player stood in the
  air at the map's origin, the server and the client disagreed about the position, and the character fell off the
  zeppelin. The loading screen also released around that wrong point, which is why the new location seemed to load
  suspiciously fast.
- **The fix.** `SMSG_TRANSFER_PENDING` now keeps its optional transport entry (and the old map id). When the following
  `SMSG_NEW_WORLD` belongs to a transport transfer, `GameSession::takeTransportSpawn()` hands the offset to the game
  loop instead of moving the mover to it; the old queued movement packets are dropped. `main.cpp` waits (under the
  loading screen) until the transport object with that entry is in the new map, takes the pose the client computes
  from `transports.txt`, converts the offset to a world position and only then swaps the map pack and starts loading
  the ground around it. The mover gets the world position, the transport guid and the local offset in one heartbeat,
  so every later movement packet keeps the player on the deck.
- **During the wait** the ride and boarding code is skipped (the old transport is not in the new map yet), movement
  and jumping are blocked, and the 25-second "transfer never arrived" fallback switches the map even when the
  transport object never comes (it logs the entry and uses the offset as the only data left).
- **The loading gate treats the deck as the floor.** While a ride is known, `streamerFloorHeight` under a zeppelin out
  over the sea no longer has to find terrain within 4 yards: the transport's deck is the floor, and the gate waits for
  the terrain and collision around the transport's real position instead. This is the other half of the "loads too
  fast" symptom.
- Tests: `net/tests/run_transportworld.sh` (13 checks: a plain transfer is still a world position; a transport transfer
  keeps the entry and the local offset, does not move the mover and is handed out once; the next transfer is plain
  again). `net/tests/run_transport_tests.sh` (spline / transport / transport-object) still passes; the 3DS build
  compiles and links (`make`, `w3dproto.3dsx`, `w3dproto.cia`). Not tried on a live server yet.
