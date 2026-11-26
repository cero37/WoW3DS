#include "../game.hpp"

#include <cassert>

using namespace w3n;

int main() {
    static_assert(UF::OBJECT_ENTRY == 3 && UF::UNIT_BYTES_0 == 23 && UF::UNIT_HEALTH == 24);
    static_assert(UF::UNIT_MAXHEALTH == 32 && UF::UNIT_LEVEL == 54 && UF::UNIT_FACTIONTEMPLATE == 55);
    static_assert(UF::UNIT_DISPLAYID == 67 && UF::UNIT_NATIVEDISPLAYID == 68 && UF::PLAYER_VISIBLE_ITEM_1_0 == 283);
    GameSession game;
    Talent talent;
    talent.id = 9001;
    talent.tree = 1;
    talent.rankSpell[0] = 1001;
    talent.rankSpell[1] = 1002;
    talent.rankSpell[2] = 1003;
    game.talents.add({1, 1, 0, "Test tree"});
    game.talents.add(talent);

    Writer initial;
    initial.u8(0);
    initial.u16(2);
    initial.u32(0x12345678);
    initial.u16(1);
    initial.u32(0x87654321);
    initial.u16(2);
    initial.u16(1);
    initial.u32(0x12345678);
    initial.u16(0);
    initial.u16(5);
    initial.u32(3000);
    initial.u32(0);
    assert(game.handle(0x12A, initial.b, 1000));
    assert(game.spells.size() == 2);
    assert(game.spells[0] == 0x12345678 && game.spells[1] == 0x87654321);
    assert(game.cooldownLeftMs(0x12345678, 1000) == 3000);

    Writer truncatedCooldowns;
    truncatedCooldowns.u8(0);
    truncatedCooldowns.u16(1);
    truncatedCooldowns.u32(99);
    truncatedCooldowns.u16(0);
    truncatedCooldowns.u16(1);
    truncatedCooldowns.u32(1000);
    assert(game.handle(0x12A, truncatedCooldowns.b, 1050));
    assert(game.spells.size() == 2 && game.spells[0] == 0x12345678);

    Writer truncatedSpells;
    truncatedSpells.u8(0);
    truncatedSpells.u16(1);
    truncatedSpells.u32(42);
    assert(game.handle(0x12A, truncatedSpells.b, 1100));
    assert(game.spells.size() == 2 && game.spells[0] == 0x12345678);

    Writer talents;
    talents.u8(0);
    talents.u32(7);
    talents.u8(2);
    talents.u8(1);
    talents.u8(1);
    talents.u32(9001);
    talents.u8(1);
    talents.u8(6);
    for (uint16_t glyph = 11; glyph < 17; ++glyph) talents.u16(glyph);
    talents.u8(1);
    talents.u32(9001);
    talents.u8(3);
    talents.u8(6);
    for (uint16_t glyph = 21; glyph < 27; ++glyph) talents.u16(glyph);
    assert(game.handle(0x4C0, talents.b, 1200));
    assert(game.talentPoints() == 7);
    assert(game.talentRank(talent) == 3);
    for (uint8_t slot = 0; slot < 6; ++slot) assert(game.glyphId(slot) == 21u + slot);

    Writer malformedTalents;
    malformedTalents.u8(0);
    malformedTalents.u32(0);
    malformedTalents.u8(1);
    malformedTalents.u8(0);
    malformedTalents.u8(1);
    malformedTalents.u32(9001);
    malformedTalents.u8(6);
    assert(game.handle(0x4C0, malformedTalents.b, 1300));
    assert(game.talentRank(talent) == 3 && game.glyphId(0) == 21);

    Writer petTalents;
    petTalents.u8(1);
    petTalents.u32(0);
    petTalents.u8(1);
    petTalents.u8(0);
    petTalents.u8(0);
    petTalents.u8(0);
    assert(game.handle(0x4C0, petTalents.b, 1400));
    assert(game.talentRank(talent) == 3 && game.glyphId(0) == 21);

    Bytes sent;
    uint16_t sentOpcode = 0;
    game.tap = [&](uint32_t, int direction, uint16_t op, const Bytes& payload) {
        if (direction == 1) { sentOpcode = op; sent = payload; }
    };
    Writer sync;
    sync.u32(0xAABBCCDD);
    sync.u32(0x01020304);
    assert(game.handle(0x390, sync.b, 4321));
    Reader response(sent);
    assert(sentOpcode == 0x391 && response.u32() == 0xAABBCCDD && response.u32() == 4321 && !response.bad &&
           response.left() == 0);

    Writer truncatedAccountTimes;
    truncatedAccountTimes.u32(0);
    truncatedAccountTimes.u8(0);
    truncatedAccountTimes.u32(0xFF);
    assert(game.handle(0x209, truncatedAccountTimes.b, 1500));

    Writer truncatedFactions;
    truncatedFactions.u32(2);
    truncatedFactions.u8(0);
    truncatedFactions.u32(0);
    assert(game.handle(0x122, truncatedFactions.b, 1600));
    assert(!game.rep.initialized);
}
