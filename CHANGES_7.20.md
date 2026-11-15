# 7.20 — the client confirms the end of a server spline (Charge, knockback)

- vMaNGOS can start a spline on a player (Charge through `MotionMaster::MoveCharge`). From that moment it sets
  `SplineDonePending` and drops **every** movement packet we send (`WorldSession::HandleMovementOpcodes`: `if
  (pMover->HasPendingSplineDone()) return;`) until `CMSG_MOVE_SPLINE_DONE` arrives with the same spline id and our
  position at the end of the spline.
- The client never sent that ack. The charge looked right, but the server still had the player where the spline began:
  charges and normal blows did not land (the range was measured from there), the grid around the new place never loaded
  (no mobs came in), and a relog put the character back at the charge's start.
- `SMSG_MONSTER_MOVE` and the movement block of `SMSG_UPDATE_OBJECT` now keep the spline id (`WorldObject::pathSplineId`),
  and `GameSession::endSelfServerMove()` sends `CMSG_MOVE_SPLINE_DONE` — the movement info at the spline's end, the id,
  and the float the 1.12 client appends (the server reads past it). A taxi is not acknowledged: the server ends it
  itself and the client never confirmed those either.
- Tests: `selfmove_test` grew to 13 checks — the ack is sent when the self spline ends, its position is the spline's end
  (20 yd), the id is the packet's (1), the body is movement info + id + float (36 bytes), and a taxi spline sends nothing.
  `autoattack_test`, `feed_test`, `pet_test`, `selfcast_test`, `cooldown_test` and `useitem_test` still pass; `proto3ds`
  links. Not tried against a live server yet.
