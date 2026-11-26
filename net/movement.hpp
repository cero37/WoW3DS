// movement.hpp — WotLK 3.3.5a MovementInfo and the client-side mover that
// turns "where I am / which keys are held" into MSG_MOVE_* packets the way the
// stock client does (start/stop on key changes, heartbeat every 500 ms while
// moving, SET_FACING when turning).
//
// Wire layout (TrinityCore/AzerothCore 3.3.5a):
//   u32 flags, u16 extraFlags, u32 time, f32 x, y, z, o,
//   [ONTRANSPORT: packed guid, f32 x, y, z, o, u32 time, i8 seat, [u32 time2]],
//   [SWIMMING|FLYING|ALWAYS_ALLOW_PITCHING: f32 pitch], u32 fallTime,
//   [FALLING: f32 zspeed, sin, cos, xyspeed], [SPLINE_ELEVATION: f32]
// Movement packets include the mover's packed GUID before MovementInfo.
#pragma once
#include <cstdint>
#include <vector>

#include "wire.hpp"

namespace w3n {

namespace MoveFlag {
enum : uint32_t {
    Forward = 0x1, Backward = 0x2, StrafeLeft = 0x4, StrafeRight = 0x8,
    TurnLeft = 0x10, TurnRight = 0x20, PitchUp = 0x40, PitchDown = 0x80,
    WalkMode = 0x100, OnTransport = 0x200, Levitating = 0x400, Root = 0x800,
    Falling = 0x1000, Jumping = Falling, FallingFar = 0x2000, Swimming = 0x200000, Ascending = 0x400000,
    Descending = 0x800000, CanFly = 0x1000000, Flying = 0x2000000,
    SplineElevation = 0x4000000, SplineEnabled = 0x8000000,
    WaterWalking = 0x10000000, SafeFall = 0x20000000, Hover = 0x40000000,
    MaskMoving = Forward | Backward | StrafeLeft | StrafeRight,
    MaskTurning = TurnLeft | TurnRight,
};
}

namespace MoveExtraFlag {
enum : uint16_t {
    AlwaysAllowPitching = 0x0020,
    InterpolatedMovement = 0x0400,
};
}

// WotLK 3.3.5a movement opcodes.
namespace MoveOp {
enum : uint16_t {
    StartForward = 0x0B5, StartBackward = 0x0B6, Stop = 0x0B7, StartStrafeLeft = 0x0B8, StartStrafeRight = 0x0B9,
    StopStrafe = 0x0BA, Jump = 0x0BB, StartTurnLeft = 0x0BC, StartTurnRight = 0x0BD, StopTurn = 0x0BE,
    StartPitchUp = 0x0BF, StartPitchDown = 0x0C0, StopPitch = 0x0C1, SetRunMode = 0x0C2, SetWalkMode = 0x0C3,
    Teleport = 0x0C5, TeleportAck = 0x0C7, FallLand = 0x0C9, StartSwim = 0x0CA, StopSwim = 0x0CB,
    SetFacing = 0x0DA, SetPitch = 0x0DB, WorldportAck = 0x0DC, Heartbeat = 0x0EE,
    Root = 0x0EC, Unroot = 0x0ED,
};
// True for the MSG_MOVE_* opcodes the server relays from other players.
bool isRelayedMove(uint16_t op);
const char* name(uint16_t op);
}

struct MovementInfo {
    uint32_t flags = 0, time = 0;
    uint16_t extraFlags = 0;
    float x = 0, y = 0, z = 0, o = 0;
    uint64_t transportGuid = 0;
    float tx = 0, ty = 0, tz = 0, to = 0;
    uint32_t transportTime = 0, transportTime2 = 0;
    int8_t transportSeat = -1;
    float pitch = 0;
    uint32_t fallTime = 0;
    float jumpZSpeed = 0, jumpSin = 0, jumpCos = 0, jumpXYSpeed = 0;
    float splineElevation = 0;
};

void writeMovementInfo(Writer& w, const MovementInfo& m);
bool readMovementInfo(Reader& r, MovementInfo& m);

struct MoveSpeeds {
    float walk = 2.5f, run = 7.0f, runBack = 4.5f, swim = 4.722222f, swimBack = 2.5f, turn = 3.141594f;
    float flight = 7.0f, flightBack = 4.5f, pitch = 3.141594f;
    // Ground speed for a set of movement flags (backward wins over forward, as in the client).
    float forFlags(uint32_t flags) const;
};

struct OutPacket {
    uint16_t opcode;
    Bytes data;
};

// Client-side mover. Feed it the local player's state every frame; it queues the
// packets the stock client would send. The caller owns physics (position, ground
// height) and must move the player at speeds.forFlags(flags) or less.
class Mover {
public:
    void setGuid(uint64_t guid) { guid_ = guid; }
    void reset(float x, float y, float z, float o, uint32_t nowMs);
    // dirFlags: any of Forward/Backward/StrafeLeft/StrafeRight (others ignored).
    void update(uint32_t dirFlags, float x, float y, float z, float o, uint32_t nowMs);
    void jump(uint32_t nowMs);              // MSG_MOVE_JUMP (caller simulates the arc)
    // 6.18d: walked off a ledge (or came out of the water over nothing): the falling flag goes out with a heartbeat, like the
    // stock client; the server measures the fall from it and deals fall damage when land() reports the landing.
    void fall(uint32_t nowMs);
    // 6.19b: on the way down (after a jump's apex or off a ledge): MOVEFLAG_FALLINGFAR (0x4000) joins the jumping flag, with a
    // heartbeat at once. vMaNGOS Player::HandleFall deals no fall damage unless the last packet before FALL_LAND had it.
    void descend(uint32_t nowMs);
    void land(float z, uint32_t nowMs);     // MSG_MOVE_FALL_LAND
    void stopAll(uint32_t nowMs);           // release every key (e.g. leaving walk mode)
    void setRooted(bool rooted);
    // Walk mode (stage 5.11, stick pushed only partway): MSG_MOVE_SET_WALK_MODE / SET_RUN_MODE on a change; the flag
    // stays in every movement packet and the speed becomes speeds.walk (also backwards, as the server counts it).
    void setWalk(bool on, uint32_t nowMs);
    bool walking() const { return (m_.flags & MoveFlag::WalkMode) != 0; }
    // Server-toggled states (SMSG_MOVE_WATER_WALK / FEATHER_FALL / SET_HOVER and their opposites): the flag is
    // carried in every movement packet from now on, as the stock client does. Kept across reset() (teleports).
    void setFlag(uint32_t flag, bool on);
    static constexpr uint32_t kStickyFlags = MoveFlag::WaterWalking | MoveFlag::SafeFall | MoveFlag::Hover;
    bool rooted() const { return (m_.flags & MoveFlag::Root) != 0; }
    bool airborne() const { return (m_.flags & MoveFlag::Jumping) != 0; }

    // Swimming (stage 5.26). The flag may only appear with MSG_MOVE_START_SWIM and go with MSG_MOVE_STOP_SWIM (vMaNGOS
    // MovementAnticheat: any other packet changing it is a flag-change cheat). START_SWIM also ends a jump or fall
    // (a fall-end opcode like FALL_LAND, carrying the fall time; the server deals no fall damage for it). While
    // swimming the packets carry the pitch: MSG_MOVE_SET_PITCH when it changed, and a heartbeat also when only the
    // height changed (swimming straight up or down).
    void startSwim(uint32_t nowMs);
    void stopSwim(uint32_t nowMs);
    void setSwimPitch(float pitch, uint32_t nowMs);
    bool swimming() const { return (m_.flags & MoveFlag::Swimming) != 0; }
    float pitchEpsilon = 0.08f;             // radians before a SET_PITCH is sent

    // Standing on a transport (stage 5.21): MoveFlag::OnTransport with the guid and the offset on it go into every packet;
    // x / y / z given to update() stay world coordinates (the server takes the offset). A teleport (reset) clears it.
    void setTransport(uint64_t guid, float lx, float ly, float lz, float lo, uint32_t nowMs);   // boarded: sends a heartbeat
    void setTransportOffset(float lx, float ly, float lz, float lo);                             // every frame while aboard
    void clearTransport(uint32_t nowMs);                                                        // stepped off: heartbeat
    bool onTransport() const { return (m_.flags & MoveFlag::OnTransport) != 0; }

    // Current movement info stamped with nowMs (used by force-change ACKs).
    MovementInfo snapshot(uint32_t nowMs) const;
    const MovementInfo& info() const { return m_; }
    std::vector<OutPacket>& queue() { return out_; }

    MoveSpeeds speeds;
    uint32_t heartbeatMs = 500;
    float facingEpsilon = 0.10f;            // radians before a SET_FACING is sent
    uint32_t facingIntervalMs = 100;
    uint32_t packetsSent = 0, heartbeats = 0;

private:
    void emit(uint16_t op, uint32_t nowMs);
    MovementInfo m_;
    uint64_t guid_ = 0;
    uint32_t lastSendMs_ = 0, jumpStartMs_ = 0;
    float lastSentO_ = 0, lastSentZ_ = 0, lastSentPitch_ = 0;
    std::vector<OutPacket> out_;
};

float normalizeAngle(float a);   // [0, 2*pi)

}  // namespace w3n
