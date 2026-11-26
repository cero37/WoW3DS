#include "../client.hpp"
#include "../inflate.hpp"
#include "../wire.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <chrono>
#include <string>
#include <thread>

using namespace w3n;

namespace {

bool readExact(int fd, uint8_t* data, size_t size) {
    size_t read = 0;
    while (read < size) {
        ssize_t n = ::recv(fd, data + read, size - read, 0);
        if (n <= 0) return false;
        read += (size_t)n;
    }
    return true;
}

bool sendExact(int fd, const uint8_t* data, size_t size) {
    size_t sent = 0;
    while (sent < size) {
        ssize_t n = ::send(fd, data + sent, size - sent, 0);
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    return true;
}

void put16(Bytes& data, uint16_t value) {
    data.push_back((uint8_t)value);
    data.push_back((uint8_t)(value >> 8));
}

void put32(Bytes& data, uint32_t value) {
    put16(data, (uint16_t)value);
    put16(data, (uint16_t)(value >> 16));
}

void put64(Bytes& data, uint64_t value) {
    put32(data, (uint32_t)value);
    put32(data, (uint32_t)(value >> 32));
}

bool receiveClientPacket(int fd, HeaderCipher& cipher, bool encrypted, uint32_t& opcode, Bytes& body) {
    uint8_t header[6];
    if (!readExact(fd, header, sizeof header)) return false;
    if (encrypted) cipher.encrypt(header, sizeof header);
    const uint16_t size = (uint16_t)((header[0] << 8) | header[1]);
    if (size < 4 || size > 4096) return false;
    opcode = (uint32_t)header[2] | ((uint32_t)header[3] << 8) | ((uint32_t)header[4] << 16) |
             ((uint32_t)header[5] << 24);
    body.resize(size - 4);
    return body.empty() || readExact(fd, body.data(), body.size());
}

bool verifyAddonInfo(const Bytes& authSession, const Bytes& sessionKey) {
    Reader packet(authSession);
    if (packet.u32() != 12340 || packet.u32() != 0 || packet.cstr() != "TEST" || packet.u32() != 0)
        return false;
    Bytes clientSeed = packet.bytes(4);
    if (packet.u32() != 0 || packet.u32() != 0 || packet.u32() != 0 || packet.u32() != 3)
        return false;
    Bytes digest = packet.bytes(20);
    Writer digestInput;
    digestInput.raw("TEST", 4);
    digestInput.u32(0);
    digestInput.raw(clientSeed);
    digestInput.raw(Bytes{1, 2, 3, 4});
    digestInput.raw(sessionKey);
    if (digest != sha1(digestInput.b) || packet.left() < 4) return false;
    const uint32_t unpackedSize = packet.u32();
    if (unpackedSize != 465) return false;
    const size_t pos = packet.pos;
    if (pos + 2 > authSession.size() || authSession[pos] != 0x78 || authSession[pos + 1] != 0x9C)
        return false;

    Bytes addon;
    if (!zlibInflate(authSession.data() + pos, authSession.size() - pos, addon, unpackedSize))
        return false;
    Reader r(addon);
    static const char* const officialAddons[] = {
        "Blizzard_AchievementUI", "Blizzard_ArenaUI", "Blizzard_AuctionUI",
        "Blizzard_BattlefieldMinimap", "Blizzard_BindingUI", "Blizzard_CombatLog",
        "Blizzard_CombatText", "Blizzard_GlyphUI", "Blizzard_InspectUI",
        "Blizzard_MacroUI", "Blizzard_RaidUI", "Blizzard_TalentUI",
        "Blizzard_TimeManager", "Blizzard_TradeSkillUI", "Blizzard_TrainerUI",
        "Blizzard_VehicleUI"
    };
    if (r.u32() != sizeof(officialAddons) / sizeof(officialAddons[0])) return false;
    for (const char* name : officialAddons) {
        if (r.cstr() != name || r.u8() != 1 || r.u32() != 0x4C1C776D || r.u32() != 0) return false;
    }
    return !r.bad && r.u32() != 0 && r.left() == 0;
}

void appendServerPacket(Bytes& out, HeaderCipher& cipher, uint16_t opcode, const Bytes& payload) {
    const uint16_t size = (uint16_t)(payload.size() + 2);
    uint8_t header[] = {
        (uint8_t)(size >> 8), (uint8_t)size, (uint8_t)opcode, (uint8_t)(opcode >> 8)
    };
    cipher.decrypt(header, sizeof header);
    out.insert(out.end(), header, header + sizeof header);
    out.insert(out.end(), payload.begin(), payload.end());
}

Bytes characterList() {
    Bytes body;
    body.push_back(9);
    for (uint8_t i = 0; i < 9; ++i) {
        put64(body, 0x1000 + i);
        const std::string name = "Hero" + std::to_string(i);
        body.insert(body.end(), name.begin(), name.end());
        body.push_back(0);
        body.insert(body.end(), 9, 0);   // race through level
        body.insert(body.end(), 8, 0);   // zone and map
        body.insert(body.end(), 12, 0);  // position
        body.insert(body.end(), 12, 0);  // guild, flags, customize flags
        body.push_back(0);               // first login
        body.insert(body.end(), 12, 0);  // pet data
        for (int slot = 0; slot < 23; ++slot) {
            put32(body, 0);
            body.push_back(0);
            put32(body, 0);
        }
    }
    body.resize(32767, 0);
    return body;
}

bool runServer(int listenFd, const Bytes& sessionKey, std::string& error) {
    const int fd = ::accept(listenFd, nullptr, nullptr);
    if (fd < 0) { error = "accept failed"; return false; }

    Bytes challenge(40, 0);
    challenge[4] = 1; challenge[5] = 2; challenge[6] = 3; challenge[7] = 4;
    Bytes challengeFrame = {0, 42, 0xEC, 0x01};
    challengeFrame.insert(challengeFrame.end(), challenge.begin(), challenge.end());
    if (!sendExact(fd, challengeFrame.data(), challengeFrame.size())) {
        error = "could not send auth challenge";
        ::close(fd);
        return false;
    }

    uint32_t opcode = 0;
    Bytes authSession;
    HeaderCipher unencrypted;
    if (!receiveClientPacket(fd, unencrypted, false, opcode, authSession) || opcode != 0x1ED ||
        !verifyAddonInfo(authSession, sessionKey)) {
        error = "invalid CMSG_AUTH_SESSION or AddonInfo";
        ::close(fd);
        return false;
    }

    HeaderCipher cipher;
    cipher.init(sessionKey);
    Bytes authResponse = {0x0C, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2};
    Bytes responseBurst;
    appendServerPacket(responseBurst, cipher, 0x1EE, authResponse);
    appendServerPacket(responseBurst, cipher, 0x2EF, Bytes{0x11});
    appendServerPacket(responseBurst, cipher, 0x255, Bytes{0x22});
    appendServerPacket(responseBurst, cipher, 0x0FD, Bytes{0x33});
    if (!sendExact(fd, responseBurst.data(), responseBurst.size())) {
        error = "could not send the encrypted auth response burst";
        ::close(fd);
        return false;
    }

    Bytes charEnum;
    if (!receiveClientPacket(fd, cipher, true, opcode, charEnum) || opcode != 0x37 || !charEnum.empty()) {
        error = "invalid encrypted CMSG_CHAR_ENUM";
        ::close(fd);
        return false;
    }

    Bytes chars = characterList();
    const size_t size = chars.size() + 2;
    Bytes header = {(uint8_t)(0x80 | (size >> 16)), (uint8_t)(size >> 8), (uint8_t)size, 0x3B, 0x00};
    cipher.decrypt(header.data(), 4);
    cipher.decrypt(header.data() + 4, 1);
    bool sent = sendExact(fd, header.data(), 4);
    if (sent) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    sent = sent && sendExact(fd, header.data() + 4, 1) && sendExact(fd, chars.data(), chars.size());
    if (!sent) error = "could not send extended SMSG_CHAR_ENUM";
    if (sent) {
        uint8_t byte;
        while (::recv(fd, &byte, sizeof byte, 0) > 0) {}
    }
    ::close(fd);
    return sent;
}

}  // namespace

int main() {
    const int listener = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) return 1;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (::bind(listener, (sockaddr*)&address, sizeof address) != 0 || ::listen(listener, 1) != 0) {
        ::close(listener);
        return 1;
    }
    socklen_t addressSize = sizeof address;
    if (::getsockname(listener, (sockaddr*)&address, &addressSize) != 0) {
        ::close(listener);
        return 1;
    }

    Bytes sessionKey(40);
    for (size_t i = 0; i < sessionKey.size(); ++i) sessionKey[i] = (uint8_t)i;
    std::string serverError;
    bool serverOk = false;
    std::thread server([&] { serverOk = runServer(listener, sessionKey, serverError); });

    WorldClient client;
    const bool started = client.start("127.0.0.1:" + std::to_string(ntohs(address.sin_port)), "TEST", sessionKey);
    std::vector<uint16_t> receivedBurst;
    client.onPacket = [&](const Packet& packet) { receivedBurst.push_back(packet.opcode); };
    const uint64_t deadline = nowMs() + 10000;
    while (started && client.state() != WorldClient::State::CharSelect &&
           client.state() != WorldClient::State::Failed && nowMs() < deadline) {
        client.update();
        sleepMs(1);
    }
    const bool receivedNine = client.state() == WorldClient::State::CharSelect && client.characters().size() == 9;
    const bool receivedBurstInOrder =
        receivedBurst == std::vector<uint16_t>{0x2EF, 0x255, 0x0FD};
    client.close();
    if (!started) ::shutdown(listener, SHUT_RDWR);
    server.join();
    ::close(listener);

    if (!started || !receivedNine || !receivedBurstInOrder || !serverOk) {
        std::fprintf(stderr, "world auth handshake test failed: client=%s, server=%s\n",
                     client.error().c_str(), serverError.c_str());
        return 1;
    }
    std::puts("WotLK world auth handshake passed (16-addon AuthSession, encrypted response burst, CHAR_ENUM)");
    return 0;
}
