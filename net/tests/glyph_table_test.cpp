#include "../tables.hpp"

#include <cassert>
#include <cstdio>

int main() {
    const char* path = "/tmp/w3d_wotlk_glyphs_test.txt";
    FILE* file = std::fopen(path, "wb");
    assert(file);
    std::fputs("B\t1\t32\t0\tBlood\nG\t1234\t5678\n", file);
    std::fclose(file);

    w3n::TalentTable talents;
    assert(talents.load(path));
    assert(talents.glyphSpell(1234) == 5678);
    std::remove(path);
}
