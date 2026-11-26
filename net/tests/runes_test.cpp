#include "../game.hpp"

#include <cassert>

int main() {
    w3n::GameSession game;
    w3n::Writer resync;
    resync.u32(6);
    for (uint8_t i = 0; i < 6; ++i) {
        resync.u8(i % 4);
        resync.u8((uint8_t)(i * 40));
    }
    assert(game.handle(0x487, resync.b, 100));
    for (size_t i = 0; i < game.runes.size(); ++i) {
        assert(game.runes[i].seen);
        assert(game.runes[i].type == i % 4);
        assert(game.runes[i].progress == i * 40);
    }

    w3n::Writer convert;
    convert.u8(2);
    convert.u8(3);
    assert(game.handle(0x486, convert.b, 200));
    assert(game.runes[2].type == 3);

    w3n::Writer power;
    power.u32(0x15);
    assert(game.handle(0x488, power.b, 300));
    assert(game.runePowerMask == 0x15 && game.runePowerMs == 300);

    w3n::Writer malformed;
    malformed.u32(7);
    assert(game.handle(0x487, malformed.b, 400));
    assert(game.runes[2].type == 3);
}
