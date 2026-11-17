# 7.21 — floating numbers for spell damage, and Thunder Clap without a target

- **The floating combat numbers now cover every blow, not only melee swings.** `GameSession::hits` (the list
  `hud_game.cpp` draws over the units) was filled by `SMSG_ATTACKERSTATEUPDATE` alone. Direct spell damage
  (`SMSG_SPELLNONMELEEDAMAGELOG`: Fireball, Thunder Clap, wands, Arcane Shot ...), damage-over-time ticks
  (`SMSG_PERIODICAURALOG`: Rend, Serpent Sting, Corruption ...) and damage shields (`SMSG_SPELLDAMAGESHIELD`: Thorns,
  Retribution Aura) go into the same list now. Spell criticals are read from the packet's hit-type flags
  (`SPELL_HIT_TYPE_CRIT 0x2`), so they still show the orange `!`; the damage-over-time and shield packets carry no
  critical bit. Only the blows we deal or take are added, as before.
- **Thunder Clap was refused in combat, but worked with nothing selected.** The client sends the selected enemy as the
  unit target only when it does not know the spell as self-centred (`spells.txt` `@self`). The converter's list of
  caster-centred implicit targets was `1, 20, 30, 33, 34, 36`; Thunder Clap's real 1.12 data uses `22` (the source
  location, the caster) and `15` (the enemies around the source), so it was missing from `spells.txt`. With an enemy
  selected the client sent that enemy to a spell whose `Targets` mask is 0; the server's target-mask check answered
  `SPELL_FAILED_BAD_TARGETS` ("Invalid target"). Without a target the cast went out and worked. The converter adds
  `15` and `22` now, which also marks Frost Nova, Arcane Explosion, Whirlwind, Psychic Scream and the other
  caster-centred areas (810 more spells in the 1.12 `Spell.dbc` of the tested client, none of them with a unit target
  in its mask).
- **An old `spells.txt` still works.** When a harmful spell without `@range` data is refused `BAD_TARGETS` at a
  selected enemy, the client retries it once with no target at all (the friendly `@friend` fallback of 7.11 is left
  as it was). A ranged or helpful spell is never retried this way. The retry is forgotten after the next successful
  cast, so the same spell may be retried again later.
- **What to do on the console:** rerun the converter's interface/texts step (`ui.bat` / `w3dconv --ui`, or
  `w3dconv --texts-only`) so the new `spells.txt` has the `@self` lines, and copy it to `sdmc:/3ds/w3dproto/`. An
  immediate workaround for an already converted card is to append `@self\t6343`, `@self\t8198`, `@self\t8204`,
  `@self\t8205`, `@self\t11580`, `@self\t11581` (the Thunder Clap ranks) to `spells.txt` — the client fallback above
  covers them until then.
- Tests: `net/tests/run_spelldamage.sh` (9 checks: direct spell damage, a critical, damage on us, a stranger's spell,
  a short packet, two damage-over-time ticks, a damage shield, the chat line) and `net/tests/run_selfarea.sh`
  (8 checks: the enemy target on the first Thunder Clap cast, the no-target retry after `BAD_TARGETS`, no second
  retry, no retry for a ranged or a helpful spell). `selfcast_test` (4 checks) and `pet_test` (67 checks) still pass;
  the converter's `--texts-only` test now expects `@self` for Thunder Clap and the self-centred nova. Built with the
  Windows/Linux `net/` sources and the Linux converter from this tree; not tried against a live server yet.
