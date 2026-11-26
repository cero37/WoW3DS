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
    static const uint8_t officialAddonInfoCompressed[] = {
        0x78, 0x9c, 0x75, 0xd2, 0xc1, 0x4e, 0xc3, 0x30, 0x0c, 0xc6, 0xf1, 0xf0, 0x14, 0x5c,
        0xf6, 0x32, 0x74, 0x48, 0x53, 0xa5, 0xe5, 0x42, 0xc3, 0x19, 0xb9, 0xc9, 0x47, 0x63,
        0x9a, 0x38, 0x55, 0x9a, 0x8e, 0x6d, 0xef, 0x88, 0x78, 0x25, 0xc4, 0x0d, 0x24, 0xf7,
        0xfc, 0xb3, 0x6c, 0xe9, 0x2f, 0x3f, 0x1a, 0x63, 0xba, 0xc4, 0xf7, 0x3b, 0xd5, 0xf0,
        0xf6, 0xe4, 0x23, 0xe3, 0x82, 0x0c, 0x69, 0xaf, 0xbd, 0x79, 0xc8, 0x9f, 0x87, 0xb3,
        0xf9, 0xe7, 0x15, 0x42, 0xba, 0x6c, 0xbe, 0x71, 0x11, 0xd5, 0x3a, 0xaa, 0x23, 0xea,
        0x1a, 0xcb, 0xb2, 0xc3, 0xad, 0x25, 0xbc, 0x33, 0x52, 0xb0, 0x2c, 0x9c, 0x69, 0xd1,
        0x86, 0x58, 0x02, 0xcb, 0xa4, 0x2e, 0x38, 0x52, 0x82, 0x04, 0xaa, 0x1a, 0x95, 0x3c,
        0x52, 0x3b, 0x97, 0x69, 0xd7, 0x1c, 0xae, 0x4d, 0xc1, 0x67, 0x8c, 0xdb, 0xe4, 0x4a,
        0x49, 0xab, 0x82, 0x27, 0x7b, 0x8c, 0xa4, 0x17, 0x3a, 0xd9, 0x61, 0xab, 0x17, 0xdc,
        0x74, 0x4c, 0xb7, 0x25, 0xea, 0xb2, 0x71, 0x0a, 0x1d, 0xc9, 0xac, 0x6a, 0x2f, 0xeb,
        0x02, 0xaf, 0x1f, 0xec, 0x1b, 0xf2, 0x50, 0xfc, 0x8c, 0xb6, 0x97, 0xc7, 0x92, 0xaf,
        0x45, 0x95, 0x17, 0xe2, 0xa0, 0x82, 0xfb, 0x2d, 0xaa, 0xdf, 0x73, 0x9c, 0x61, 0x49,
        0x68, 0x82, 0xd6, 0xdb, 0x95, 0x19, 0xfa, 0x13, 0xb8, 0x4a, 0x01, 0xc3, 0xcc, 0x29,
        0xed, 0x31, 0x0b, 0xea, 0x5f, 0xfb, 0x8a, 0xdf, 0x1f, 0x3f, 0xe4, 0x89, 0xc9, 0x9c,
    };
    if (authSession.size() != 279) return false;
    Reader packet(authSession);
    if (packet.u32() != 12340 || packet.u32() != 0 || packet.cstr() != "B3H3MOTH" || packet.u32() != 0)
        return false;
    Bytes clientSeed = packet.bytes(4);
    if (packet.u32() != 0 || packet.u32() != 0 || packet.u32() != 0 || packet.u64() != 0)
        return false;
    Bytes digest = packet.bytes(20);
    Writer digestInput;
    digestInput.raw("B3H3MOTH", 8);
    digestInput.u32(0);
    digestInput.raw(clientSeed);
    digestInput.raw(Bytes{1, 2, 3, 4});
    digestInput.raw(sessionKey);
    if (digest != sha1(digestInput.b) || packet.left() < 4) return false;
    const uint32_t unpackedSize = packet.u32();
    if (unpackedSize != 0x0000029E || packet.left() != sizeof(officialAddonInfoCompressed))
        return false;

    Bytes compressed = packet.bytes(sizeof(officialAddonInfoCompressed));
    if (packet.bad || std::memcmp(compressed.data(), officialAddonInfoCompressed,
                                  sizeof(officialAddonInfoCompressed)) != 0)
        return false;
    Bytes addon;
    return zlibInflate(compressed.data(), compressed.size(), addon, unpackedSize) &&
           addon.size() == 0x0000029E;
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
    const bool started =
        client.start("127.0.0.1:" + std::to_string(ntohs(address.sin_port)), "B3H3MOTH", sessionKey);
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
    std::puts("WotLK world auth handshake passed (captured AuthSession, encrypted response burst, CHAR_ENUM)");
    return 0;
}
