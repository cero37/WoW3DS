#include "../movement.hpp"
#include "../objects.hpp"

#include <cstdio>

using namespace w3n;

int main() {
    static_assert(MoveFlag::CanFly == 0x01000000);
    static_assert(MoveFlag::Flying == 0x02000000);
    static_assert(MoveExtraFlag::AlwaysAllowPitching == 0x0020);
    static_assert(MoveExtraFlag::InterpolatedMovement == 0x0400);
    static_assert(MoveOp::StartForward == 0x0B5);
    static_assert(ObjOp::SMSG_MONSTER_MOVE == 0x0DD);

    MovementInfo expected;
    expected.flags = MoveFlag::OnTransport;
    expected.extraFlags = MoveExtraFlag::AlwaysAllowPitching | MoveExtraFlag::InterpolatedMovement;
    expected.time = 123456;
    expected.x = 1.0f; expected.y = 2.0f; expected.z = 3.0f; expected.o = 0.5f;
    expected.transportGuid = 0x1FC0000000012345ull;
    expected.tx = -1.0f; expected.ty = 0.25f; expected.tz = 4.0f; expected.to = 1.5f;
    expected.transportTime = 654321;
    expected.transportSeat = 2;
    expected.transportTime2 = 765432;
    expected.pitch = -0.75f;
    expected.fallTime = 42;

    Writer w;
    writeMovementInfo(w, expected);
    Reader r(w.b);
    MovementInfo actual;
    if (!readMovementInfo(r, actual) || r.left() != 0 ||
        actual.flags != expected.flags || actual.extraFlags != expected.extraFlags ||
        actual.time != expected.time || actual.transportGuid != expected.transportGuid ||
        actual.transportTime != expected.transportTime || actual.transportSeat != expected.transportSeat ||
        actual.transportTime2 != expected.transportTime2 || actual.pitch != expected.pitch ||
        actual.fallTime != expected.fallTime) {
        std::puts("WotLK MovementInfo wire test failed");
        return 1;
    }

    std::puts("WotLK MovementInfo wire test passed");
    return 0;
}
