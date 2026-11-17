# 7.25 — Old 3DS: held X for the target menu, an L + Y target ring, Y tap clears the target, zoom hints

- **The target menu is reachable again on the Old 3DS.** X used to do only one thing there (the next hotbar set), while
  the menu lived on X in the New 3DS set 0. Now **a tap of X is still the next set, a hold (about 0.4 s) opens the
  target's menu** (Attack, Interact, Assist, Whisper, Invite, Trade, Follow, Duel, Inspect ...). The set changes on
  release, so a hold never switches it as well.
- **A tap of Y clears the target.** It used to open the chat. Chat stays in **START -> Chat** and on the touch Chat tab;
  a Y hold still looks around, and L + Y still aims the target ring. A tap now deselects whatever is targeted (and
  stops auto-attack, as the original's clear-selection does).
- **The camera zoom is written where it can be found.** On the Old 3DS hold **Y** and use **D-pad up / down** to zoom
  the camera out / in (the Circle Pad / D-pad left-right looks around meanwhile). The Controls page says it; the
  README's Old 3DS table has X tap/hold and Y hold/tap rows.
- **The target ring works on the Old 3DS too: hold L + Y and aim with the Circle Pad.** The same red enemy ring as the
  New 3DS C-Stick aim appears under the unit; releasing Y (or L) takes the aimed enemy. While aiming the Circle Pad
  does not walk or turn the camera, L does not also cycle the target, and the top-screen panel says
  `ENEMY: Circle Pad aims / release Y: take target`.
- The top-screen bars are unchanged: `X set N`, `L: target  R: attack`, the fixed `X Set / A Use / B Jump / Y Look`
  face diamond and the four D-pad hotbar cells.
- Tests: `pad_test` (81 checks) covers the X tap (next set), the X hold (menu once, no set change), the Y tap (clear
  target, no chat), L + Y aiming (the yaw from the Circle Pad, no walking/camera turn, no next target) and the release
  (the aimed unit, no clear). The 3DS build compiles and links (`w3dproto.3dsx`, `w3dproto.cia`). The SD card was not
  mounted at the end, so the new files still need to be copied (`sdmc:/3ds/w3dproto/w3dproto.3dsx`,
  `cias/w3dproto0.8.2-opendsp.cia`).
