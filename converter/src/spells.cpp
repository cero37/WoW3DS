// Spell names for the 3DS action bar (stage 5.8): Spell.dbc -> spells.txt, one "id<TAB>name<TAB>rank" line each.
// Faction reactions (stage 5.12): FactionTemplate.dbc + Faction.dbc -> factions.txt (see net/factions.hpp).
//
// Only spells that appear in SkillLineAbility.dbc (class, racial and profession spells a player can learn)
// are written, which is ~5 000 lines instead of ~24 000. WotLK 3.3.5a Spell.dbc has 234 fields; its localized
// name group starts at column 136 and its subtext/rank group at 153 (16 locale columns per group).
#include "conv.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <set>
#include <unordered_map>

namespace w3d {

namespace {
std::string g_localeDir = "enUS";
int g_localeIndex = 0;
}  // namespace

int gameLocaleIndex() { return g_localeIndex; }
const std::string& gameLocaleDir() { return g_localeDir; }

void setGameLocale(const std::string& code) {
    std::string c = code;
    for (char& ch : c) ch = (char)std::tolower((unsigned char)ch);
    if (c.empty() || c == "auto") {
        g_localeDir = c.empty() ? "enUS" : "auto";
        g_localeIndex = 0;
        return;
    }
    struct Entry { const char* code; int index; const char* dir; };
    static const Entry kMap[] = {
        {"enus", 0, "enUS"}, {"engb", 0, "enGB"}, {"kokr", 1, "koKR"}, {"frfr", 2, "frFR"},
        {"dede", 3, "deDE"}, {"zhcn", 4, "zhCN"}, {"zhtw", 5, "zhTW"}, {"eses", 6, "esES"},
        {"esmx", 7, "esMX"}, {"ruru", 8, "ruRU"}, {"ptbr", 9, "ptBR"}, {"itit", 10, "itIT"},
    };
    for (const Entry& e : kMap)
        if (c == e.code) {
            g_localeDir = e.dir;
            g_localeIndex = e.index;
            return;
        }
    std::fprintf(stderr, "locale: unknown '%s', using enUS\n", code.c_str());
    g_localeDir = "enUS";
    g_localeIndex = 0;
}

bool DbcTable::load(const DataDir& data, const char* name) {
    std::string path = std::string("DBFilesClient\\") + name;
    if (!data.read(path, b) || b.size() < 20 || std::memcmp(b.data(), "WDBC", 4) != 0) return false;
    std::memcpy(&records, &b[4], 4); std::memcpy(&fields, &b[8], 4);
    std::memcpy(&recSize, &b[12], 4); std::memcpy(&strSize, &b[16], 4);
    return 20 + (size_t)records * recSize + strSize <= b.size();
}

uint32_t DbcTable::u32(uint32_t r, uint32_t f) const {
    if (r >= records || f >= fields || (f + 1) * 4 > recSize) return 0;
    uint32_t v;
    std::memcpy(&v, &b[20 + (size_t)r * recSize + f * 4], 4);
    return v;
}

float DbcTable::f32(uint32_t r, uint32_t f) const {
    uint32_t v = u32(r, f);
    float x;
    std::memcpy(&x, &v, 4);
    return x;
}

std::string DbcTable::str(uint32_t r, uint32_t f) const {
    uint32_t off = u32(r, f);
    size_t base = 20 + (size_t)records * recSize;
    if (off == 0 || off >= strSize) return {};
    const char* s = (const char*)&b[base + off];
    size_t n = 0;
    while (off + n < strSize && s[n]) ++n;
    return std::string(s, n);
}

std::string DbcTable::loc(uint32_t r, uint32_t base, uint32_t count) const {
    // WotLK LocalizedString has 16 columns; esMX is index 7. Use the first
    // populated locale when the selected column is empty.
    const uint32_t locales = std::min<uint32_t>(count, 16);
    const int li = gameLocaleIndex();
    if (li >= 0 && (uint32_t)li < locales) {
        std::string s = str(r, base + (uint32_t)li);
        if (!s.empty()) return s;
    }
    for (uint32_t c = 0; c < locales; ++c) {
        std::string s = str(r, base + c);
        if (!s.empty()) return s;
    }
    return {};
}

namespace {
using Table = DbcTable;
}  // namespace

int spellNameColumn(const DbcTable& spell) {
    constexpr uint32_t name = dbc335a::spell::Name;
    return spell.fields >= 234 ? (int)name : -1;
}

bool exportSpellNames(const DataDir& data, const std::string& path, bool verbose) {
    Table spell;
    if (!spell.load(data, "Spell.dbc")) {
        std::fprintf(stderr, "spells: Spell.dbc not found (no spells.txt)\n");
        return false;
    }
    int nameCol = spellNameColumn(spell);
    if (nameCol < 0) {
        std::fprintf(stderr, "spells: Spell.dbc is not the WotLK 3.3.5a layout (no spells.txt)\n");
        return false;
    }
    constexpr uint32_t rankCol = dbc335a::spell::NameSubtext;

    std::set<uint32_t> wanted;
    Table sla;   // SkillLineAbility: ID 0, SkillLine 1, Spell 2
    bool haveSla = sla.load(data, "SkillLineAbility.dbc") && sla.fields > 2;
    if (haveSla)
        for (uint32_t r = 0; r < sla.records; ++r) wanted.insert(sla.u32(r, 2));

    std::string out;
    size_t n = 0;
    for (uint32_t r = 0; r < spell.records; ++r) {
        uint32_t id = spell.u32(r, 0);
        if (!id || (haveSla && !wanted.count(id))) continue;
        std::string name = spell.loc(r, (uint32_t)nameCol);
        if (name.empty()) continue;
        std::string rank = spell.loc(r, (uint32_t)rankCol);
        for (char& ch : name) if (ch == '\t' || ch == '\n' || ch == '\r') ch = ' ';
        for (char& ch : rank) if (ch == '\t' || ch == '\n' || ch == '\r') ch = ' ';
        out += std::to_string(id) + '\t' + name + '\t' + rank + '\n';
        ++n;
        // SchoolMask is a bit mask in WotLK; the consumer expects the school enum.
        const uint32_t mask = spell.u32(r, dbc335a::spell::SchoolMask);
        uint32_t school = 0;
        for (uint32_t bit = 1; bit <= 6; ++bit)
            if (mask == (1u << bit)) { school = bit; break; }
        if (school >= 1 && school <= 6) out += "@school\t" + std::to_string(id) + "\t" + std::to_string(school) + "\n";
    }
    // Trainers list spells that teach (effect 36 LEARN_SPELL) under the name and rank of what they teach.
    size_t taught = 0;
    if (haveSla) {
        std::unordered_map<uint32_t, uint32_t> rowOf;
        for (uint32_t r = 0; r < spell.records; ++r) rowOf[spell.u32(r, 0)] = r;
        constexpr uint32_t effCol = dbc335a::spell::Effect, trigCol = dbc335a::spell::EffectTriggerSpell;
        for (uint32_t r = 0; r < spell.records; ++r) {
            uint32_t id = spell.u32(r, 0);
            if (!id || wanted.count(id)) continue;
            for (uint32_t k = 0; k < 3; ++k) {
                if (spell.u32(r, effCol + k) != 36) continue;
                uint32_t t = spell.u32(r, trigCol + k);
                auto it = rowOf.find(t);
                if (!wanted.count(t) || it == rowOf.end()) continue;
                std::string name = spell.loc(it->second, (uint32_t)nameCol);
                std::string rank = spell.loc(it->second, (uint32_t)rankCol);
                if (name.empty()) continue;
                for (char& ch : name) if (ch == '\t' || ch == '\n' || ch == '\r') ch = ' ';
                for (char& ch : rank) if (ch == '\t' || ch == '\n' || ch == '\r') ch = ' ';
                out += std::to_string(id) + '\t' + name + '\t' + rank + '\n';
                ++taught;
                break;
            }
        }
    }
    // Buffs and debuffs (stage 5.16): the aura bar shows auras of any spell (food, NPC spells, items), so every
    // visible aura spell gets its name too: an effect 6 (APPLY_AURA) or 35 (APPLY_AREA_AURA), not passive (0x40)
    // and not hidden (0x80) in Attributes.
    size_t auras = 0;
    {
        std::set<uint32_t> written(wanted.begin(), wanted.end());
        constexpr uint32_t effCol = dbc335a::spell::Effect, attrCol = dbc335a::spell::Attributes;
        for (uint32_t r = 0; r < spell.records; ++r) {
            uint32_t id = spell.u32(r, 0);
            if (!id || written.count(id) || (spell.u32(r, attrCol) & 0xC0)) continue;
            bool aura = false;
            for (uint32_t k = 0; k < 3; ++k) aura = aura || spell.u32(r, effCol + k) == 6 || spell.u32(r, effCol + k) == 35;
            if (!aura) continue;
            std::string name = spell.loc(r, (uint32_t)nameCol);
            if (name.empty()) continue;
            for (char& ch : name) if (ch == '\t' || ch == '\n' || ch == '\r') ch = ' ';
            out += std::to_string(id) + '\t' + name + "\t\n";
            written.insert(id);
            ++auras;
        }
    }
    // UI stage 10: spells placed on the ground (Blizzard, Flamestrike, Rain of Fire, Volley, Hurricane, the engineers'
    // bombs ...): "@ground<TAB>id<TAB>max range<TAB>radius" (yards). Ground = the Targets mask asks for a destination
    // or an effect hits the units around a chosen point. WotLK Spell.dbc target/effect columns and SpellRange MaxRange are used.
    // (items' spells are not in SkillLineAbility); the old client skips these lines (no id before the first tab).
    size_t ground = 0;
    {
        Table range, radius;
        std::unordered_map<uint32_t, float> rangeOf, radiusOf;
        if (range.load(data, "SpellRange.dbc") && range.fields > 3)
            for (uint32_t r = 0; r < range.records; ++r) rangeOf[range.u32(r, 0)] = range.f32(r, 3);
        if (radius.load(data, "SpellRadius.dbc") && radius.fields > 1)
            for (uint32_t r = 0; r < radius.records; ++r) radiusOf[radius.u32(r, 0)] = radius.f32(r, 1);
        constexpr uint32_t tgtCol = 16, rangeCol = dbc335a::spell::RangeIndex;
        constexpr uint32_t aCol = dbc335a::spell::EffectImplicitTargetA, bCol = dbc335a::spell::EffectImplicitTargetB;
        constexpr uint32_t radCol = dbc335a::spell::EffectRadiusIndex;
        auto areaAtDest = [](uint32_t t) { return t == 16 || t == 28 || t == 31 || t == 34; };
        std::string known;
        for (uint32_t r = 0; r < spell.records; ++r) {
            const uint32_t id = spell.u32(r, 0);
            if (!id) continue;
            bool g = (spell.u32(r, tgtCol) & 0x40) != 0;
            float rad = 0;
            for (uint32_t k = 0; k < 3; ++k) {
                if (areaAtDest(spell.u32(r, aCol + k)) || areaAtDest(spell.u32(r, bCol + k))) g = true;
                auto it = radiusOf.find(spell.u32(r, radCol + k));
                if (it != radiusOf.end()) rad = std::max(rad, it->second);
            }
            if (!g) continue;
            auto rg = rangeOf.find(spell.u32(r, rangeCol));
            const float maxRange = rg != rangeOf.end() ? rg->second : 30.0f;
            if (maxRange <= 0.5f) continue;   // "self" range: centred on the caster, nothing to place
            char line[96];
            std::snprintf(line, sizeof line, "@ground\t%u\t%.1f\t%.1f\n", id, maxRange, rad > 0 ? rad : 5.0f);
            out += line;
            ++ground;
            if (id == 10 || id == 2120 || id == 5740 || id == 1510 || id == 16914 || id == 4064)
                known += " " + std::to_string(id) + "(" + spell.loc(r, (uint32_t)nameCol) + ")";
        }
        std::printf("  ground-targeted spells: %zu (known among them:%s)\n", ground, known.empty() ? " none" : known.c_str());
    }
    // Items used on another item (Shiny Bauble, weapon oils, sharpening stones, poisons): WotLK target/equipment fields
    // identify the selected item, while the held-item attribute requires the main-hand item.
    size_t itemCasts = 0;
    {
        constexpr uint32_t tgtCol = 16, attrCol = dbc335a::spell::Attributes, effCol = dbc335a::spell::Effect;
        std::string known;
        for (uint32_t r = 0; r < spell.records; ++r) {
            const uint32_t id = spell.u32(r, 0);
            if (!id) continue;
            const int32_t cls = (int32_t)spell.u32(r, dbc335a::spell::EquippedItemClass);
            const uint32_t sub = spell.u32(r, dbc335a::spell::EquippedItemSubclass);
            const uint32_t inv = spell.u32(r, dbc335a::spell::EquippedItemInvTypes);
            const bool mainHand = (spell.u32(r, attrCol) & 0x200) != 0;
            // The Targets mask is what the client uses to pick the cursor. Some item spells leave it 0 and only mark the
            // held item: their item requirements (a real class, not -1) and an enchant effect (53 permanent, 54 temporary,
            // 92 held item) tell them from ordinary spells (Fishing also names the main hand but summons a bobber).
            bool enchantsItem = false;
            for (uint32_t k = 0; k < 3; ++k) {
                const uint32_t e = spell.u32(r, effCol + k);
                if (e == 53 || e == 54 || e == 92) enchantsItem = true;
            }
            if (!(spell.u32(r, tgtCol) & 0x10) && !(mainHand && cls >= 0 && enchantsItem)) continue;
            char line[96];
            std::snprintf(line, sizeof line, "@item\t%u\t%d\t%u\t%u\t%u\n", (unsigned)id, (int)cls, (unsigned)sub, (unsigned)inv,
                          (unsigned)(mainHand ? 1 : 0));
            out += line;
            ++itemCasts;
            if (id == 8087) known += " " + std::to_string(id) + "(" + spell.loc(r, (uint32_t)nameCol) + ")";
        }
        std::printf("  item-targeted spells: %zu (Shiny Bauble among them:%s)\n", itemCasts, known.empty() ? " no" : known.c_str());
    }
    // 5.48: spells that need no unit target: "@self<TAB>id". Every effect's ImplicitTargetA/B (vMaNGOS SpellTarget) is the
    // caster (1), its party / the friends or hostiles around it (20, 30, 33, 34, 36) or an area at the caster's own
    // position (15: enemies around the source, 22: the source itself) - Battle Shout, Thunder Clap, Frost Nova, Arcane
    // Explosion. 7.21: 15 and 22 were missing, so Thunder Clap was sent at the selected enemy; the server expects no unit
    // target for it and refused the cast (it worked only with nothing selected). Spells with any single-unit target (6
    // chain damage, 21 friend ...) or an unknown one are left out.
    size_t selfCasts = 0;
    {
        constexpr uint32_t effC = dbc335a::spell::Effect, aC = dbc335a::spell::EffectImplicitTargetA;
        constexpr uint32_t bC = dbc335a::spell::EffectImplicitTargetB;
        auto selfKind = [](uint32_t t) {
            return t == 1 || t == 15 || t == 20 || t == 22 || t == 30 || t == 33 || t == 34 || t == 36;
        };
        for (uint32_t r = 0; r < spell.records; ++r) {
            const uint32_t id = spell.u32(r, 0);
            if (!id) continue;
            bool any = false, ok = true;
            for (uint32_t k = 0; k < 3 && ok; ++k) {
                if (!spell.u32(r, effC + k)) continue;   // no effect in this slot
                for (uint32_t t : {spell.u32(r, aC + k), spell.u32(r, bC + k)}) {
                    if (!t) continue;
                    if (selfKind(t)) any = true; else ok = false;
                }
            }
            if (!any || !ok) continue;
            out += "@self\t" + std::to_string(id) + "\n";
            ++selfCasts;
        }
        std::printf("  spells without a unit target: %zu\n", selfCasts);
    }
    // 7.11: helpful spells at a friendly unit: "@friend<TAB>id" - an effect aims at a single friend (21) or party member (57)
    // and none at a single enemy (6, 25): Holy Light, Flash Heal, Rejuvenation ...; the client casts them at itself when the
    // selected target is an enemy or nothing.
    size_t friends = 0;
    {
        constexpr uint32_t effC = dbc335a::spell::Effect, aC = dbc335a::spell::EffectImplicitTargetA;
        constexpr uint32_t bC = dbc335a::spell::EffectImplicitTargetB;
        for (uint32_t r = 0; r < spell.records; ++r) {
            const uint32_t id = spell.u32(r, 0);
            if (!id) continue;
            bool fr = false, foe = false;
            for (uint32_t k = 0; k < 3; ++k) {
                if (!spell.u32(r, effC + k)) continue;
                for (uint32_t t : {spell.u32(r, aC + k), spell.u32(r, bC + k)}) {
                    if (t == 21 || t == 57) fr = true;
                    if (t == 6 || t == 25) foe = true;
                }
            }
            if (!fr || foe) continue;
            out += "@friend\t" + std::to_string(id) + "\n";
            ++friends;
        }
        std::printf("  helpful spells at a friend: %zu\n", friends);
    }
    // UI stage 12: cooldowns, "@cd id recovery category categoryRecovery onEvent" (ms).
    size_t cds = 0;
    {
        constexpr uint32_t catCol = dbc335a::spell::Category, attrCol = dbc335a::spell::Attributes;
        constexpr uint32_t recCol = dbc335a::spell::RecoveryTime, catRecCol = dbc335a::spell::CategoryRecoveryTime;
        std::string known;
        for (uint32_t r = 0; r < spell.records; ++r) {
            const uint32_t id = spell.u32(r, 0), rec = spell.u32(r, recCol), catRec = spell.u32(r, catRecCol);
            const uint32_t cat = spell.u32(r, catCol);
            const bool onEvent = (spell.u32(r, attrCol) & 0x02000000u) != 0;
            if (!id || (!rec && !catRec && !onEvent) || rec > 86400000u || catRec > 86400000u) continue;
            char line[96];
            std::snprintf(line, sizeof line, "@cd\t%u\t%u\t%u\t%u\t%u\n", id, rec, cat, catRec, onEvent ? 1u : 0u);
            out += line;
            ++cds;
            if (id == 871 || id == 2139 || id == 1953 || id == 100 || id == 20271)
                known += " " + std::to_string(id) + "(" + spell.loc(r, (uint32_t)nameCol) + " " + std::to_string(rec ? rec : catRec) + " ms)";
        }
        std::printf("  spell cooldowns: %zu (known among them:%s)\n", cds, known.empty() ? " none" : known.c_str());
    }
    // Cast times: Spell.dbc CastingTimeIndex -> SpellCastTimes.dbc (ID, Base ms, PerLevel, Minimum).
    // The client starts its own cast bar and animation on send instead of waiting for SMSG_SPELL_START (ping).
    size_t casts = 0;
    {
        DbcTable ct;
        if (ct.load(data, "SpellCastTimes.dbc")) {
            std::unordered_map<uint32_t, uint32_t> baseOf;
            for (uint32_t r = 0; r < ct.records; ++r) {
                const int32_t base = (int32_t)ct.u32(r, 1);
                if (base > 0) baseOf[ct.u32(r, 0)] = (uint32_t)base;
            }
            constexpr uint32_t idxCol = 28;
            for (uint32_t r = 0; r < spell.records; ++r) {
                const uint32_t id = spell.u32(r, 0);
                auto it = baseOf.find(spell.u32(r, idxCol));
                if (!id || it == baseOf.end() || it->second > 60000) continue;
                out += "@cast\t" + std::to_string(id) + "\t" + std::to_string(it->second) + "\n";
                ++casts;
            }
            std::printf("  spell cast times: %zu\n", casts);
        } else {
            std::fprintf(stderr, "spells: SpellCastTimes.dbc not found (no cast prediction)\n");
        }
    }
    // WotLK does not store DmgClass in Spell.dbc; export only ranged/stop-attack behavior represented by attributes.
    size_t combat = 0;
    {
        constexpr uint32_t attrCol = dbc335a::spell::Attributes, attr2Col = dbc335a::spell::AttributesExB;
        size_t ranged = 0, stops = 0, passives = 0;
        // 6.95: "@passive id" - SPELL_ATTR_PASSIVE spells: the book shows them but they cannot be put on a bar
        for (uint32_t r = 0; r < spell.records; ++r) {
            const uint32_t id = spell.u32(r, 0);
            if (id && (spell.u32(r, attrCol) & 0x40)) { out += "@passive\t" + std::to_string(id) + "\n"; ++passives; }
        }
        std::printf("  passive spells: %zu\n", passives);
        // 6.99: "@hidden id" - spells the game's book never lists: SPELL_ATTR_DO_NOT_DISPLAY (Attributes 0x80), and the ones that
        // only grant a skill - weapon and armour proficiencies (Swords, Plate Mail), languages, skill steps (Effect 60 PROFICIENCY,
        // 39 LANGUAGE, 44 SKILL_STEP, 118 SKILL): those are skills (the Skills tab), not spells. Effect[3]: name - 56.
        size_t hidden = 0;
        for (uint32_t r = 0; r < spell.records; ++r) {
            const uint32_t id = spell.u32(r, 0);
            if (!id) continue;
            bool hide = (spell.u32(r, attrCol) & 0x80) != 0;
            if (!hide) {
                bool any = false, onlySkill = true;
                for (uint32_t k = 0; k < 3; ++k) {
                    const uint32_t e = spell.u32(r, dbc335a::spell::Effect + k);
                    if (!e) continue;
                    any = true;
                    if (e != 39 && e != 44 && e != 60 && e != 118) onlySkill = false;
                }
                hide = any && onlySkill;
            }
            if (hide) { out += "@hidden\t" + std::to_string(id) + "\n"; ++hidden; }
        }
        std::printf("  spells hidden from the book: %zu\n", hidden);
        for (uint32_t r = 0; r < spell.records; ++r) {
            const uint32_t id = spell.u32(r, 0);
            if (!id) continue;
            const uint32_t attr = spell.u32(r, attrCol), attr2 = spell.u32(r, attr2Col);
            if (attr & 0x40) continue;   // passive
            const bool learnable = !haveSla || wanted.count(id);
            uint32_t kind = 0;
            if ((attr & 0x2) || (attr2 & 0x20)) kind |= 2;
            if (learnable && (attr & 0x100000)) kind |= 4;
            if (!kind) continue;
            out += "@combat\t" + std::to_string(id) + "\t" + std::to_string(kind) + "\n";
            ++combat;
            ranged += (kind & 2) != 0; stops += (kind & 4) != 0;
        }
        std::printf("  auto-attack spells: %zu (%zu ranged, %zu that stop it; WotLK Spell.dbc has no DmgClass field)\n", combat,
                    ranged, stops);
    }
    // Range, "@range id min max" (yards; WotLK SpellRange.dbc MinRange 1, MaxRange 3).
    // The original client refuses a cast at a unit out of range / too close itself, before sending it. Index 1 (self) and 2
    // (melee "combat range": the server checks reach with the bounding radii) are left out.
    size_t ranges = 0;
    {
        Table rg;
        if (rg.load(data, "SpellRange.dbc") && rg.fields > 3) {
            std::unordered_map<uint32_t, std::pair<float, float>> mm;
            for (uint32_t r = 0; r < rg.records; ++r) mm[rg.u32(r, 0)] = {rg.f32(r, 1), rg.f32(r, 3)};
            constexpr uint32_t rangeCol = dbc335a::spell::RangeIndex;
            for (uint32_t r = 0; r < spell.records; ++r) {
                const uint32_t id = spell.u32(r, 0), idx = spell.u32(r, rangeCol);
                if (!id || idx <= 2 || (haveSla && !wanted.count(id))) continue;
                auto it = mm.find(idx);
                if (it == mm.end() || it->second.second <= 0.5f || it->second.second > 1000.0f) continue;
                char line[96];
                std::snprintf(line, sizeof line, "@range\t%u\t%.1f\t%.1f\n", id, it->second.first, it->second.second);
                out += line;
                ++ranges;
            }
        }
        std::printf("  spell ranges: %zu\n", ranges);
    }
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "spells: cannot write %s\n", path.c_str());
        return false;
    }
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    std::printf("  spells: %zu names + %zu trainer spells + %zu auras written to %s (Spell.dbc %u records, name column %d%s)\n", n, taught, auras,
                path.c_str(), spell.records, nameCol, haveSla ? ", filtered by SkillLineAbility" : ", all spells");
    (void)verbose;
    return true;
}


// Faction tables are exported from their WotLK layouts. Spell.dbc uses a 16-column locale block.

std::vector<std::string> availableLocales(const DataDir& data) {
    std::vector<std::string> out;
    DbcTable spell;
    if (!spell.load(data, "Spell.dbc")) return out;
    const int nameCol = spellNameColumn(spell);
    if (nameCol < 0 || (uint32_t)nameCol >= spell.fields) return out;
    const int cols = std::min(16, (int)(spell.fields - (uint32_t)nameCol));
    if (cols <= 0) return out;
    uint32_t baseRows = 0;
    std::vector<uint32_t> diff((size_t)cols, 0);
    for (uint32_t r = 0; r < spell.records; ++r) {
        const std::string base = spell.str(r, (uint32_t)nameCol);
        if (!base.empty()) {
            ++baseRows;
        }
        for (int c = 1; c < cols; ++c) {
            const std::string s = spell.str(r, (uint32_t)(nameCol + c));
            if (!s.empty() && s != base) ++diff[(size_t)c];
        }
    }
    static const char* const kLocales[11] = {"enUS", "koKR", "frFR", "deDE", "zhCN", "zhTW",
                                              "esES", "esMX", "ruRU", "ptBR", "itIT"};
    if (baseRows) out.push_back(kLocales[0]);
    // A handful of translated names is enough (custom clients may only translate part of the data); avoid counting
    // rows where a locale column merely repeats the English fallback.
    const uint32_t need = std::max<uint32_t>(5, baseRows / 100);
    for (int c = 1; c < cols && c < 11; ++c)
        if (diff[(size_t)c] >= need) out.push_back(kLocales[c]);
    return out;
}

bool exportFactions(const DataDir& data, const std::string& path, bool verbose) {
    Table ft, fa;
    if (!ft.load(data, "FactionTemplate.dbc") || ft.fields < 14) {
        std::fprintf(stderr, "factions: FactionTemplate.dbc not found (no factions.txt)\n");
        return false;
    }
    bool haveFa = fa.load(data, "Faction.dbc") && fa.fields >= 39;
    std::string out = "# wow3ds factions: T id faction flags our friendly hostile enemy x4 friend x4 | "
                      "F id repIndex raceMask x4 classMask x4 base x4 flags x4 name\n";
    size_t nt = 0, nf = 0;
    for (uint32_t r = 0; r < ft.records; ++r) {
        if (!ft.u32(r, 0)) continue;
        out += "T";
        for (uint32_t c = 0; c < 14; ++c) out += "\t" + std::to_string(ft.u32(r, c));
        out += "\n";
        ++nt;
    }
    if (haveFa)
        for (uint32_t r = 0; r < fa.records; ++r) {
            if (!fa.u32(r, 0)) continue;
            out += "F\t" + std::to_string(fa.u32(r, 0)) + "\t" + std::to_string((int32_t)fa.u32(r, 1));
            for (uint32_t c = 2; c < 18; ++c)
                out += "\t" + (c >= 10 && c < 14 ? std::to_string((int32_t)fa.u32(r, c)) : std::to_string(fa.u32(r, c)));
            std::string name = fa.loc(r, 23, 16);
            for (char& ch : name) if (ch == '\t' || ch == '\n' || ch == '\r') ch = ' ';
            out += "\t" + name + "\n";
            ++nf;
        }
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "factions: cannot write %s\n", path.c_str());
        return false;
    }
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    std::printf("  factions: %zu templates, %zu factions written to %s%s\n", nt, nf, path.c_str(),
                haveFa ? "" : " (Faction.dbc missing: no reputations)");
    (void)verbose;
    return true;
}

}  // namespace w3d
