#include "../client.hpp"
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

bool verifyAddonInfo(const Bytes& authSession) {
    size_t pos = 8;
    while (pos < authSession.size() && authSession[pos] != 0) ++pos;
    if (pos == authSession.size()) return false;
    pos += 1 + 48;
    if (pos + 4 + 7 + 4 > authSession.size()) return false;

    const uint32_t unpackedSize = (uint32_t)authSession[pos] |
                                  ((uint32_t)authSession[pos + 1] << 8) |
                                  ((uint32_t)authSession[pos + 2] << 16) |
                                  ((uint32_t)authSession[pos + 3] << 24);
    pos += 4;
    if (authSession[pos] != 0x78 || authSession[pos + 1] != 0x01 || authSession[pos + 2] != 0x01)
        return false;
    const uint16_t blockSize = (uint16_t)(authSession[pos + 3] | (authSession[pos + 4] << 8));
    const uint16_t inverseSize = (uint16_t)(authSession[pos + 5] | (authSession[pos + 6] << 8));
    if ((uint16_t)~blockSize != inverseSize || blockSize != unpackedSize ||
        pos + 7 + blockSize + 4 != authSession.size())
        return false;

    Bytes addon(authSession.begin() + (long)pos + 7, authSession.begin() + (long)pos + 7 + blockSize);
    Reader r(addon);
    if (r.u32() != 1 || r.cstr() != "Blizzard_CombatLog" || r.u8() != 0x01 ||
        r.u32() != 0x4C1C776D || r.u32() != 0)
        return false;
    return !r.bad && r.u32() != 0 && r.left() == 0;
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
        !verifyAddonInfo(authSession)) {
        error = "invalid CMSG_AUTH_SESSION or AddonInfo";
        ::close(fd);
        return false;
    }

    HeaderCipher cipher;
    cipher.init(sessionKey);
    Bytes authResponse = {0x0C, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2};
    Bytes responseHeader = {0, 13, 0xEE, 0x01};
    cipher.decrypt(responseHeader.data(), responseHeader.size());
    responseHeader.insert(responseHeader.end(), authResponse.begin(), authResponse.end());
    if (!sendExact(fd, responseHeader.data(), responseHeader.size())) {
        error = "could not send SMSG_AUTH_RESPONSE";
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
    const uint64_t deadline = nowMs() + 10000;
    while (started && client.state() != WorldClient::State::CharSelect &&
           client.state() != WorldClient::State::Failed && nowMs() < deadline) {
        client.update();
        sleepMs(1);
    }
    const bool receivedNine = client.state() == WorldClient::State::CharSelect && client.characters().size() == 9;
    client.close();
    if (!started) ::shutdown(listener, SHUT_RDWR);
    server.join();
    ::close(listener);

    if (!started || !receivedNine || !serverOk) {
        std::fprintf(stderr, "world auth handshake test failed: client=%s, server=%s\n",
                     client.error().c_str(), serverError.c_str());
        return 1;
    }
    std::puts("WotLK world auth handshake passed (AddonInfo, AUTH_RESPONSE, CHAR_ENUM, extended header)");
    return 0;
}
