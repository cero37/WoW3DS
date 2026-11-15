# WoW3DS 0.8.0 — release notes (12 November 2026)

This is the 0.8.0 tree plus the fixes from the 7.17 / 7.18 session. The client needs the new interface/text
step (`ui.bat` or the GUI's **Interface and texts**) once: it writes `glue.txt` with the game language and the
`enchant_glow` sprite into `ui.w3dp`.

## Controls (7.17)

- **Settings → Controls** names the schemes after the consoles: *Forever* is now **New 3DS**, *Classic* is **Old 3DS**.
  The layouts are unchanged; `pad.txt` keeps `scheme 0` / `scheme 1`, so old settings files read the same.
- **Old 3DS: Settings → Controls → D-pad → Camera / Spells.** The default *Camera* keeps the old layout (D-pad left /
  right looks, up / down zooms). With *Spells* the D-pad becomes four hotbar cells of the set shown on the touch bar
  (`X` still changes the set), and the camera is looked with the **Circle Pad while Y is held**; the D-pad looks and
  zooms as before in that mode. A tap of Y without moving the Circle Pad still opens the chat; L (next target), R
  (attack), A (interact), B (jump) and START keep their actions.
- `pad.txt` gains `dpadspells 0|1`; the Old 3DS hotbar set now lives in the `Pad` (`PadOut::set` for both schemes), so
  the top bars, the touch bar and the actual slots always agree.
- The top-screen bars name the Old 3DS buttons by their real actions (X *Set*, A *Use*, B *Jump*, Y *Chat* — *Look*
  with D-pad = Spells); the binder's cell labels follow the same rules.

## Items used on other items (7.16)

- A lure can be applied to the fishing pole; the same path covers weapon oils, sharpening stones and poisons.
- `w3dconv` writes `@item` lines into `spells.txt` (item class / subclass mask / inventory-type mask and the held-item
  attribute); the client sends `CMSG_USE_ITEM` with `TARGET_FLAG_ITEM` and the packed item GUID.
- **Bags → Use** on a use-on-item item opens a picker of the items it fits (worn ones first, with slot names). A hotbar
  slot opens the same picker.
- **Item cards show the enchant and its time left** (`Enchant: Fishing Lure +25 (9m 59s left)`) from
  `ITEM_FIELD_ENCHANTMENT` and `SMSG_ITEM_ENCHANT_TIME_UPDATE`; the time counts down while the card is open.
- The converter GUI's **Check available languages** runs `w3dconv --list-locales`, so the dropdown holds only the
  languages the client really carries; the client's `WTF\Config.wtf` language is preselected once.

## Enchanted items are visible (7.18)

- An item with an enchantment now gets a **green glow around its icon**: bags, paper doll, bank, auction, trade, the
  enchant / use-on-item pickers and the item card. `InvItem::enchanted()` reads the seven enchantment slots.
- The soft glow uses the client's `Interface\Buttons\CheckButtonGlow.blp` sprite, added to the interface pack as
  `enchant_glow` and drawn through the new additive draw mode (`kHudAdd`). A pack made before 7.18 has no sprite, so the
  client falls back to a soft green halo — the enchantment is visible either way.

## Game language reaches the server (7.18)

- The interface step writes the chosen game language into `glue.txt` (`L ruRU`, `L esES`, ...). The client reads it and
  sends that locale in the login challenge instead of always `enUS`.
- The server picks its own quest, NPC-name and gossip texts by that language. This is why a Russian run of the converter
  now gives Russian quests and names as well as the Russian map and spell names it already had. Client-side DBC texts
  and maps already followed `--locale`.
- An older `glue.txt` without the `L` line falls back to the interface language (Settings → Interface → Language), which
  also makes testing possible before the interface step is run again. A Turtle client keeps its `enUS` unless `glue.txt`
  names a language.
- This is separate from the app's own interface language (`pad.txt language 0..4`).

## Converter and the release pack

- `w3dconv`, `w3dgui`, `w3docc` and `w3dpreview` are rebuilt from this tree on 12.11 with clang-cl and the **static C++
  runtime** (no VC++ redistributable needed; only Windows system DLLs).
- The default UI skin now includes `enchant_glow` from `Interface\Buttons\CheckButtonGlow.blp`.
- `ui.bat` passes `--locale` (default `ruRU` in this pack, second argument overrides) and lists every text file to copy;
  `all.bat` and `convert.bat` also take a locale argument.
- `w3dgui.ini` has `locale=ruRU` and an empty output folder; the pack includes `w3dlogin.exe` (for `login_test.bat`) and
  `README_RU.txt`.

## Tests

- `pad_test`: 75 checks, 0 failed (D-pad slots, set switching, Y held to look vs. a Y tap for chat, stale Y release,
  D-pad camera while Y is held, `dpadspells` in the `pad.txt` round trip).
- `useitem_test`: 38 checks, 0 failed (main-hand selection, item-target mask/GUID, `ITEM_FIELD_ENCHANTMENT`, countdown,
  tooltip line, `InvItem::enchanted()`).
- `glue_test`: passes, including the `L ruRU` line.
- `w3dconv --list-locales` against the Stonetavern client: `enUS`, `ruRU`.
- `w3dconv --ui --locale ruRU` against the Stonetavern client: `glue.txt` starts with `L ruRU`, the skin has the new
  sprite; 48 undecodable icons and 6 missing skin entries are pre-existing client gaps.
- `make` in `proto3ds`: builds and links `w3dproto.cia` (178 MB memory mode) and `w3dproto-124.cia` (124 MB mode).

Not yet tried on real hardware: the Old 3DS D-pad option, the enchant glow on the console, and the `ruRU` login against a
live server.

## Files

| File | What |
|---|---|
| `w3dproto.cia` | the client, 178 MB memory mode (New 3DS / New 2DS XL) |
| `w3dproto0.8.0.cia` | the same file under the release name |
| `w3dproto-124.cia` | the 124 MB memory mode (normal New 3DS mode, own title ID: installs next to the 178 MB one) |
| `w3dproto0.8.0-124.cia` | the same 124 MB file under the release name |
| `w3dproto.3dsx` | Homebrew Launcher build |
| `Convert_pack.0.8.0.zip` | the Windows converter (`w3dconv`, `w3dgui`, `w3docc`, `w3dpreview`, bats) |
| `questdb.bin` | the quest database (`tools/questdb`) |
