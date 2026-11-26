// objects.hpp — WotLK 3.3.5a world object table: SMSG_(COMPRESSED_)UPDATE_OBJECT,
// SMSG_DESTROY_OBJECT, SMSG_MONSTER_MOVE / SMSG_COMPRESSED_MOVES, relayed
// MSG_MOVE_* of other players, and name/creature/gameobject query answers.
//
// Layouts follow TrinityCore/AzerothCore 3.3.5a update and movement packet writers.
// Positions are server coordinates (== render coordinates in wow3ds).
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "movement.hpp"
#include "spline.hpp"
#include "transport.hpp"

namespace w3n {

enum class ObjType : uint8_t { Object = 0, Item = 1, Container = 2, Unit = 3, Player = 4, GameObject = 5,
                               DynamicObject = 6, Corpse = 7 };
const char* objTypeName(ObjType t);

struct AuraState {
    uint8_t slot = 0, flags = 0, level = 0, stacks = 0;
    uint32_t spell = 0, maxDuration = 0, duration = 0;
    uint32_t updatedMs = 0;
    uint64_t caster = 0;
};

// Update field indices for 3.3.5a (TrinityCore UpdateFields.h, build 12340).
namespace UF {
enum : uint16_t {
    OBJECT_GUID = 0, OBJECT_TYPE = 2, OBJECT_ENTRY = 3, OBJECT_SCALE_X = 4,
    UNIT_FIELD_SUMMON = 8, UNIT_TARGET = 18, UNIT_FIELD_CHANNEL_OBJECT = 20, UNIT_CHANNEL_SPELL = 22,
    UNIT_BYTES_0 = 23, UNIT_HEALTH = 24, UNIT_POWER1 = 25, UNIT_MAXHEALTH = 32, UNIT_MAXPOWER1 = 33,
    UNIT_LEVEL = 54, UNIT_FACTIONTEMPLATE = 55, UNIT_VIRTUAL_ITEM_SLOT_DISPLAY = 56, UNIT_FLAGS = 59,
    UNIT_BOUNDINGRADIUS = 65, UNIT_COMBATREACH = 66, UNIT_DISPLAYID = 67, UNIT_NATIVEDISPLAYID = 68,
    UNIT_MOUNTDISPLAYID = 69, UNIT_BYTES_1 = 74, UNIT_FIELD_PETNUMBER = 75, UNIT_DYNAMIC_FLAGS = 79,
    UNIT_NPC_FLAGS = 82, UNIT_NPC_EMOTESTATE = 83, UNIT_END = 148,
    PLAYER_FLAGS = 150, PLAYER_BYTES = 153, PLAYER_BYTES_2 = 154,
    PLAYER_VISIBLE_ITEM_1_0 = 283, PLAYER_VISIBLE_ITEM_STRIDE = 2,
    PLAYER_FIELD_INV_SLOT_HEAD = 324,
    PLAYER_XP = 634, PLAYER_NEXT_LEVEL_XP = 635, PLAYER_SKILL_INFO_1_1 = 636,
    PLAYER_CHARACTER_POINTS1 = 1020, PLAYER_REST_STATE_EXPERIENCE = 1169, PLAYER_FIELD_COINAGE = 1170,
    PLAYER_FIELD_GLYPH_SLOTS = 1312, PLAYER_FIELD_GLYPHS = 1318, PLAYER_GLYPHS_ENABLED = 1324,
    PLAYER_END = 1326,
    // WotLK no longer stores aura slots in update fields; aura state comes from aura packets.
    UNIT_FIELD_AURA = 0xFFFF, UNIT_FIELD_AURAFLAGS = 0xFFFF,
    UNIT_FIELD_AURALEVELS = 0xFFFF, UNIT_FIELD_AURAAPPLICATIONS = 0xFFFF,
    // items and bags
    ITEM_FIELD_OWNER = 6, ITEM_FIELD_CONTAINED = 8, ITEM_FIELD_STACK_COUNT = 14, ITEM_FIELD_ENCHANTMENT = 22,
    ITEM_FIELD_ENCHANTMENT_STRIDE = 3, ITEM_FIELD_ENCHANTMENT_COUNT = 12, ITEM_FIELD_DURABILITY = 60,
    ITEM_FIELD_MAXDURABILITY = 61, CONTAINER_FIELD_NUM_SLOTS = 64, CONTAINER_FIELD_SLOT_1 = 66,
    CORPSE_FIELD_OWNER = 6, CORPSE_FIELD_DISPLAY_ID = 10, CORPSE_FIELD_ITEM = 11, CORPSE_FIELD_BYTES_1 = 30,
    CORPSE_FIELD_BYTES_2 = 31, CORPSE_FIELD_GUILD = 32, CORPSE_FIELD_FLAGS = 33, CORPSE_FIELD_DYNAMIC_FLAGS = 34,
    DYNAMICOBJECT_CASTER = 6, DYNAMICOBJECT_BYTES = 8, DYNAMICOBJECT_SPELLID = 9, DYNAMICOBJECT_RADIUS = 10,
    GAMEOBJECT_CREATED_BY = 6, GAMEOBJECT_DISPLAYID = 8, GAMEOBJECT_FLAGS = 9, GAMEOBJECT_ROTATION = 10,
    GAMEOBJECT_DYN_FLAGS = 14, GAMEOBJECT_STATE = 17,
    GAMEOBJECT_TYPE_ID = 18, // local cache slot; not sent in WotLK update fields
};
}


struct WorldObject {
    uint64_t guid = 0;
    ObjType type = ObjType::Object;
    bool self = false;
    bool hasPos = false;
    MovementInfo mv;                 // last authoritative movement (position, flags, orientation)
    uint32_t posMs = 0;              // local time when 'mv' was received
    MoveSpeeds speeds;
    // Server-driven spline (SMSG_MONSTER_MOVE or SPLINE_ENABLED in a movement block).
    std::vector<Vec3f> path;         // start point first
    uint32_t pathStartMs = 0, pathDurMs = 0;
    uint32_t pathSplineId = 0;       // 7.20: the server's spline id (CMSG_MOVE_SPLINE_DONE echoes it back)
    bool pathActive = false, pathHasFinalO = false;
    bool pathSmooth = false;         // flying spline (taxi flights): Catmull-Rom through the points, like the server's MoveSpline
    float pathFinalO = 0;
    // Transports (stage 5.21). A GO transport: the server's path progress when its create block arrived (UPDATEFLAG_TRANSPORT).
    uint32_t transportProgress = 0;
    bool hasTransportProgress = false;
    uint32_t gameObjectType = 0;
    bool hasGameObjectType = false;
    // A path transport remains known after OUT_OF_RANGE (often a continent change).
    // Keep its real GUID and phase, but never use its stale position without a path pose.
    bool transportOutOfRange = false;
    bool restoredTransport = false;   // real GUID / phase from a validated previous-session snapshot
    // 7.28: a transport the client put on its path itself, because the server did not announce it yet (the phase comes from
    // the previous session's memory). Dropped as soon as the real object arrives, and not drawn outside its own map.
    bool predicted = false;
    float stationary[4] = {0, 0, 0, 0};   // position + facing in the create block of a transport (UPDATEFLAG_TRANSPORT): where an elevator rests
    // A unit standing on a transport (MoveFlag::OnTransport): ObjectTable::tickTransports keeps mv at the world position
    // computed from the transport's pose and the unit's offset, and 'carried' turns the usual extrapolation off.
    bool carried = false;
    // 6.83: a new server path starts where the server says the unit is, not where we drew it: the difference (drawn - server) is
    // carried along and fades out over kBlendMs, so a chasing monster whose path is renewed every few hundred ms does not jump back and forth.
    float blend[3] = {0, 0, 0};
    uint32_t blendMs = 0;            // local time of the renewal (0 = none)
    static constexpr uint32_t kBlendMs = 350;
    uint32_t carriedStamp = 0, carriedBaseMs = 0;   // mv.time of the last packet seen, local time it was seen
    float carriedBase[3] = {0, 0, 0}, carriedO = 0;               // offset (x, y, z) in that packet
    std::vector<uint32_t> fields;    // update fields (index -> value), grown on demand
    std::vector<AuraState> auras;
    std::string name, subname;       // from the query caches
    uint8_t rank = 0;                // creatures (SMSG_CREATURE_QUERY_RESPONSE): 0 normal, 1 elite, 2 rare elite, 3 boss, 4 rare
    uint8_t creatureType = 0;        // 1 beast, 2 dragonkin, 3 demon, 4 elemental, 5 giant, 6 undead, 7 humanoid, 8 critter ...
    // Melee (stage 5.7, from SMSG_ATTACKSTART/STOP/ATTACKERSTATEUPDATE): drives attack animations.
    uint64_t meleeVictim = 0;        // who it is swinging at (0 = not in melee)
    uint32_t swingMs = 0;            // local time of its last swing (0 = never)
    bool swingOff = false;           // that swing was the off hand (HITINFO_LEFTSWING)
    // Animation events (the animation stage; local times, 0 = never): drive creature_view.cpp
    uint32_t hitMs = 0;              // struck in melee
    uint8_t hitReact = 0;            // 1 wound, 2 critical, 3 dodge, 4 parry, 5 block
    uint32_t emoteMs = 0, emote = 0; // SMSG_EMOTE: a one-shot Emotes.dbc id
    uint32_t castGoMs = 0;           // a spell went off (SMSG_SPELL_GO)
    uint32_t customAnimMs = 0;       // game objects: SMSG_GAMEOBJECT_CUSTOM_ANIM (the bobber's splash when a fish bites)
    uint32_t castGoSpell = 0;
    // Game objects (stage 5.41): when GAMEOBJECT_STATE last changed (local ms; 0 = it was like that when we first saw it),
    // so the door / chest animation plays only for a change we saw happen.
    uint32_t stateChangeMs = 0;
    bool castGoDirected = false;     // at someone else (the hands point) or around itself
    bool castGoAtObject = false;     // 6.36: at a game object (opening, gathering): the caster kneels instead of casting
    bool taxiFlight() const { return isUnitLike() && (field(UF::UNIT_FLAGS) & 0x00100000u) != 0; }   // UNIT_FLAG_TAXI_FLIGHT
    uint8_t standState() const { return (uint8_t)(field(UF::UNIT_BYTES_1) & 0xFF); }   // 0 stand, 1 sit, 3 sleep, 8 kneel ...
    uint32_t emoteState() const { return field(UF::UNIT_NPC_EMOTESTATE); }             // looping Emotes.dbc id (work, dance)

    uint32_t field(uint16_t i) const { return i < fields.size() ? fields[i] : 0; }
    float fieldF(uint16_t i) const;
    uint32_t entry() const { return field(UF::OBJECT_ENTRY); }
    uint32_t level() const { return field(UF::UNIT_LEVEL); }
    uint32_t health() const { return field(UF::UNIT_HEALTH); }
    uint32_t maxHealth() const { return field(UF::UNIT_MAXHEALTH); }
    uint32_t displayId() const { return type == ObjType::GameObject ? field(UF::GAMEOBJECT_DISPLAYID) : field(UF::UNIT_DISPLAYID); }
    bool isUnitLike() const { return type == ObjType::Unit || type == ObjType::Player; }
    bool dead() const { return isUnitLike() && fields.size() > UF::UNIT_HEALTH && health() == 0; }
    // Interpolated position at local time nowMs (splines, or players extrapolated from their key state).
    void positionAt(uint32_t nowMs, float out[3], float* orient = nullptr) const;
    // 7.13: the spline has not reached its end yet (pathActive stays set after it has: a self path is cleared by
    // GameSession::endSelfServerMove).
    bool pathRunning(uint32_t nowMs) const {
        return pathActive && pathDurMs && (int32_t)(nowMs - pathStartMs) < (int32_t)pathDurMs;
    }
    // Ground speed (yd/s) at local time nowMs: spline speed while a server path runs,
    // key-state speed for players, else 0. Drives walk/run animation choice.
    float groundSpeedAt(uint32_t nowMs) const;
    bool swimming() const { return (mv.flags & MoveFlag::Swimming) != 0; }

    // ---- game objects (stage 5.41)
    uint32_t goType() const { return hasGameObjectType ? gameObjectType : field(UF::GAMEOBJECT_TYPE_ID); }
    bool isPathTransport() const {
        return type == ObjType::GameObject && hasTransportProgress &&
               (goType() == 15 || (guid >> 48) == 0x1FC0u);
    }
    // GAMEOBJECT_BYTES_1 stores the WotLK gameobject state in its low byte.
    uint32_t goState() const { return field(UF::GAMEOBJECT_STATE) & 0xFF; }
    // GAMEOBJECT_ROTATION: the quaternion (x y z w) the server places the model with. For an upright object it is only the
    // facing about Z (sin, cos of half the angle in z / w); with x or y set the object stands tilted (logs, planks, camp parts).
    // Returns false when the quaternion is missing or invalid.
    bool goRotation(float q[4]) const;
    // The same, only when the object really is tilted (|x| or |y| beyond a small bound): then q is the full orientation.
    bool goTilt(float q[4]) const;
    // ---- dynamic objects (areas of ground spells: Blizzard, Consecration ...)
    uint64_t dynCaster() const { return (uint64_t)field(UF::DYNAMICOBJECT_CASTER) | ((uint64_t)field(UF::DYNAMICOBJECT_CASTER + 1) << 32); }
    uint32_t dynSpell() const { return field(UF::DYNAMICOBJECT_SPELLID); }
    float dynRadius() const { return fieldF(UF::DYNAMICOBJECT_RADIUS); }
    // ---- corpses
    uint64_t corpseOwner() const { return (uint64_t)field(UF::CORPSE_FIELD_OWNER) | ((uint64_t)field(UF::CORPSE_FIELD_OWNER + 1) << 32); }
    uint32_t corpseDisplay() const { return field(UF::CORPSE_FIELD_DISPLAY_ID); }
    uint32_t corpseFlags() const { return field(UF::CORPSE_FIELD_FLAGS); }   // 1 = bones (a ghost's body that has been released and decayed), 4 = lootable
    bool corpseBones() const { return (corpseFlags() & 1u) != 0; }
};

struct ObjectStats {
    uint32_t updatePackets = 0, compressedPackets = 0, failedPackets = 0;
    uint32_t blocks[6] = {0, 0, 0, 0, 0, 0};   // VALUES, MOVEMENT, CREATE, CREATE2, OUT_OF_RANGE, NEAR
    uint32_t destroys = 0, monsterMoves = 0, monsterMoveFails = 0, compressedMoves = 0, playerMoves = 0;
    uint32_t turtleFallbacks = 0, turtleCompressedMoves = 0;   // 6.58: blocks read with 1.12's layout after Turtle's failed; zlib monster moves
    uint32_t nameAnswers = 0, creatureAnswers = 0, gameobjectAnswers = 0;
    uint64_t inflatedIn = 0, inflatedOut = 0;
    std::string lastError;
};

class ObjectTable {
public:
    void clear();
    void setSelf(uint64_t guid) { self_ = guid; }
    uint64_t self() const { return self_; }
    // Returns true when the opcode belongs to the object layer (consumed).
    bool handle(uint16_t opcode, const Bytes& data, uint32_t nowMs);
    // Parse a raw (already inflated) SMSG_UPDATE_OBJECT body.
    bool parseUpdate(const uint8_t* p, size_t n, uint32_t nowMs);

    WorldObject* find(uint64_t g);
    const WorldObject* find(uint64_t g) const;
    const std::unordered_map<uint64_t, WorldObject>& all() const { return objs_; }
    size_t count(ObjType t) const;

    // Objects created since the last call (the session sends name/creature queries for them).
    std::vector<uint64_t> takeCreated() { std::vector<uint64_t> v; v.swap(created_); return v; }
    // 7.27: every create / loss of a transport object, for the perf log - it answers "did the client have the zeppelin
    // while it stood at the stop".
    struct TransportEvent {
        uint32_t ms = 0;
        uint8_t kind = 0;   // 0 create, 1 out of range, 2 destroyed, 3 replaced, 4 retained out of range
        uint64_t guid = 0;
        uint32_t entry = 0, display = 0, progress = 0;
        float x = 0, y = 0, z = 0;
    };
    std::vector<TransportEvent> takeTransportEvents() { std::vector<TransportEvent> v; v.swap(transportEvents_); return v; }
    // 7.28: every block of every SMSG_UPDATE_OBJECT, for the perf log (what the server really sent, blocked by blocked).
    // kind 0: a block (type: 0 values, 1 movement, 2 create, 3 out of range, 4 near); kind 1: a packet summary.
    struct BlockNote {
        uint8_t kind = 0, type = 0, objType = 0;
        uint64_t guid = 0;
        uint32_t entry = 0, display = 0, size = 0;
    };
    std::vector<BlockNote> takeBlockNotes() { std::vector<BlockNote> v; v.swap(blockNotes_); return v; }
    // 7.28: transports from the previous session - the client puts them on their paths itself when the server is silent,
    // and drops them the moment a real create for the same entry arrives.
    struct TransportMemory { uint32_t entry = 0, display = 0, progress = 0; uint64_t guid = 0; };
    // All progress values are measured at nowMs, NOT at each object's last create packet.
    std::vector<TransportMemory> transportMemory(uint32_t nowMs) const;
    uint64_t restoreTransport(const TransportMemory& memory, uint32_t nowMs);
    void dropRestoredTransports();
    uint64_t addPredictedTransport(uint32_t entry, uint32_t display, uint32_t progressMs, uint32_t nowMs);
    void dropPredictedTransports(uint32_t entry);   // entry 0: all of them
    bool hasTransportEntry(uint32_t entry) const;
    bool hasPlayerName(uint64_t g) const { return playerNames_.count(g) != 0; }
    bool hasCreatureInfo(uint32_t entry) const { return creatureNames_.count(entry) != 0; }
    bool hasGameObjectInfo(uint32_t entry) const { return goNames_.count(entry) != 0; }
    // SMSG_GAMEOBJECT_QUERY_RESPONSE: type and the 24 data values (chest / goober / quest giver: data[0] = lock id)
    struct GoInfo { uint32_t type = 0; uint32_t data[24] = {}; };
    void setGoInfo(uint32_t entry, const GoInfo& gi);
    const GoInfo* goInfo(uint32_t entry) const { auto it = goInfo_.find(entry); return it == goInfo_.end() ? nullptr : &it->second; }
    // Best known name of any GUID (players from the name cache even when out of range); "" if unknown.
    std::string nameOf(uint64_t g) const;

    // Transports (stage 5.21): transports.txt is read on first use; call tickTransports once per frame.
    void setTransportsFile(const std::string& path) { transportsFile_ = path; transportsTried_ = false; }
    TransportTable transports;
    // Moves every known ship / zeppelin (GO type 15) along its path and carries the units standing on them.
    void tickTransports(uint32_t nowMs);
    // Pose of a transport as of the last tick (null for other objects).
    const TransportPose* transportPose(uint64_t guid) const {
        auto it = poses_.find(guid);
        return it == poses_.end() ? nullptr : &it->second;
    }
    const std::unordered_map<uint64_t, TransportPose>& transportPoses() const { return poses_; }

    ObjectStats stats;
    // Hook for self-movement packets that need an answer (teleports are handled by the session).
    uint32_t maxFieldsOther = UF::PLAYER_END;   // fields kept for objects other than self

private:
    bool parseMovementBlock(Reader& r, WorldObject& o, uint32_t nowMs);
    bool parseMovementOrCreate(Reader& r, WorldObject& o, uint32_t nowMs, bool withFields);
    bool parseFields(Reader& r, WorldObject* o);
    uint32_t nowMs_ = 0;
    bool parseMonsterMove(Reader& r, uint32_t nowMs, bool transportPacket = false);
    void remove(uint64_t g);
    void applyNames(WorldObject& o);
    uint32_t fieldCap(const WorldObject& o) const;

    uint64_t self_ = 0;
    std::unordered_map<uint64_t, WorldObject> objs_;
    std::unordered_map<uint64_t, std::string> playerNames_;
    std::unordered_map<uint32_t, std::pair<std::string, std::string>> creatureNames_;
    std::unordered_map<uint32_t, std::pair<uint8_t, uint8_t>> creatureRank_;   // entry -> rank, type
    std::unordered_map<uint32_t, std::string> goNames_;
    std::unordered_map<uint32_t, GoInfo> goInfo_;
    std::vector<uint64_t> created_;
    std::vector<TransportEvent> transportEvents_;
    std::vector<BlockNote> blockNotes_;
    void noteBlock(const BlockNote& n);
    void noteTransport(uint8_t kind, const WorldObject& o, uint64_t guid);
    std::unordered_map<uint64_t, TransportPose> poses_;
    std::string transportsFile_;
    bool transportsTried_ = false;
};

// Object-layer opcodes.
namespace ObjOp {
enum : uint16_t {
    CMSG_NAME_QUERY = 0x050, SMSG_NAME_QUERY_RESPONSE = 0x051,
    CMSG_GAMEOBJECT_QUERY = 0x05E, SMSG_GAMEOBJECT_QUERY_RESPONSE = 0x05F,
    CMSG_CREATURE_QUERY = 0x060, SMSG_CREATURE_QUERY_RESPONSE = 0x061,
    SMSG_UPDATE_OBJECT = 0x0A9, SMSG_DESTROY_OBJECT = 0x0AA, SMSG_MONSTER_MOVE = 0x0DD,
    SMSG_MONSTER_MOVE_TRANSPORT = 0x2AE,
    SMSG_COMPRESSED_UPDATE_OBJECT = 0x1F6, SMSG_COMPRESSED_MOVES = 0x2FB,
    SMSG_AURA_UPDATE_ALL = 0x495, SMSG_AURA_UPDATE = 0x496,
};
}

}  // namespace w3n
