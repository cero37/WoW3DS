# 7.22 — the control hints follow the chosen scheme (Old 3DS / New 3DS)

- **The Old 3DS D-pad is always the hotbar.** The old *Controls -> D-pad -> Camera / Spells* option is gone: it was a
  dead layout (the D-pad looked around while there was no other way to reach a hotbar). The four D-pad cells are the
  four slots of the set X picks, as on the New 3DS. Hold Y to look with the Circle Pad (the D-pad still looks / zooms
  while Y is held); a tap of Y is chat. The setting is not written to `pad.txt` any more, and old files with
  `dpadspells` are read without it.
- **The top-screen controller bars change with Settings -> Controls -> Scheme.** The Old 3DS has no ZL/ZR and no
  C-Stick, so the bars no longer draw the ZL/ZR boxes or the `p1` page label there. The face diamond shows the fixed
  actions (X *Set*, A *Use*, B *Jump*, Y *Look* — a tap of it is chat) and the set indicator says `X set N`
  (X cycles the four sets). The D-pad diamond always shows the four hotbar cells, and a small `L: target  R: attack`
  line names the shoulder buttons (the New 3DS shows those modes only while held).
- **The interact prompt names the right button.** The name plate and game-object prompts say `[A]` on the Old 3DS
  (A is interact there) and stay `[Y]` on the New 3DS.
- **The ground-spell hint and aiming.** The Old 3DS hint says `Circle Pad: aim | A: cast`: while a ring is being
  placed the Circle Pad moves it and the character stands still (the camera was already frozen during placement), so
  the stick is not also walking. The New 3DS keeps `C-Stick aim | again: cast | cancel`.
- **The hotbar binder follows the scheme.** On the Old 3DS its rows are labelled `X1`..`X4` (X changes the set), the
  face buttons are shown fixed and greyed in every set, and the four D-pad cells are bindable. A drag-drop onto a
  fixed Old 3DS button is refused as well.
- Tests: the 3DS build compiles and links (`make`, `w3dproto.3dsx`, `w3dproto.cia`). `proto3ds/tests/run_hud.sh` now
  writes `hud.png` (New 3DS) and `hud_old.png` (Old 3DS) from the same recorded session; the PC HUD test learned
  `W3D_OLD3DS`. The Old 3DS mode was not tried on real hardware yet.
