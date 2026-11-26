#include "../crypto.hpp"

#include <cstdio>

using namespace w3n;

int main() {
    Bytes sessionKey(40);
    for (size_t i = 0; i < sessionKey.size(); ++i) sessionKey[i] = (uint8_t)i;

    HeaderCipher client;
    HeaderCipher server;
    client.init(sessionKey);
    server.init(sessionKey);
    if (!client.enabled() || !server.enabled()) return 1;

    Bytes clientHeader = {0x00, 0x18, 0x01, 0x00, 0x00, 0x00};
    client.encrypt(clientHeader.data(), clientHeader.size());
    const Bytes expectedClientHeader = {0x9B, 0xF1, 0x53, 0x52, 0x18, 0xFA};
    if (clientHeader != expectedClientHeader) {
        std::puts("WotLK client-to-server ARC4 vector mismatch");
        return 1;
    }

    Bytes serverHeader = {0x06, 0xDD, 0x27, 0x35};
    server.decrypt(serverHeader.data(), serverHeader.size());
    const Bytes expectedServerHeader = {0x18, 0x00, 0x01, 0x00};
    if (serverHeader != expectedServerHeader) {
        std::puts("WotLK server-to-client ARC4 vector mismatch");
        return 1;
    }

    HeaderCipher split;
    split.init(sessionKey);
    Bytes splitHeader = {0x00, 0x18, 0x01, 0x00, 0x00, 0x00};
    split.encrypt(splitHeader.data(), 2);
    split.encrypt(splitHeader.data() + 2, splitHeader.size() - 2);
    if (splitHeader != expectedClientHeader) {
        std::puts("WotLK ARC4 stream state was not preserved between calls");
        return 1;
    }

    std::puts("WotLK ARC4/HMAC known-answer tests passed");
    return 0;
}
