# 7.18 — enchanted items glow on their icons, and the game language reaches the server

- **The game language now reaches the server.** The converter's interface step writes the chosen game language into
  `glue.txt` (`L ruRU`, `L esES`, ...); the client reads it and sends it in the login challenge instead of always
  `enUS`. The server picks its own quest, NPC-name and gossip texts by that language, so a Russian run of the converter
  now gives Russian quests and names as well as the Russian map and spell names it already had. An older `glue.txt`
  without the line falls back to the interface language (Settings → Interface → Language), which also makes testing
  possible before the interface step is run again.
- **An item with an enchantment is now visible at a glance.** A fishing pole with a lure (or a weapon with an oil, a
  sharpening stone, a poison) gets a green glow around its icon in the bags, on the paper doll, in the bank, the auction
  and trade windows, in the enchant / use-on-item pickers and on the item card. The green `Enchant: Fishing Lure +25
  (9m 59s left)` line of the card stays as before.
- The data was already there since 7.16 (`ITEM_FIELD_ENCHANTMENT`): `InvItem::enchanted()` now says whether any of the
  seven enchantment slots holds an id, and the UI draws the glow before the icon.
- The glow is the client's own `Interface\Buttons\CheckButtonGlow` sprite, added by the interface step to `ui.w3dp` as
  `enchant_glow` and drawn additively (the new `kHudAdd` draw mode; its black adds nothing). A pack made before 7.18 has
  no such sprite, so the client falls back to a soft green halo around the icon — the enchantment is shown either way.
  Rerun the interface step (`ui.bat` / `w3dconv --ui`) to get the glow from the client's own texture.
- Tests: the glue parser was checked with a synthetic `L ruRU` line (`1 ruRU`); `net/tests/run_stage5.sh`'s
  `useitem_test` now also checks `InvItem::enchanted()` (38 checks, pass); the 3DS build compiles and links. The glow
  was previewed from the client's own texture; not yet seen on the console. The locale was not tried against a live
  server yet.
