#include "conv.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
void appendU32(std::vector<uint8_t>& out, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) out.push_back((uint8_t)(v >> (i * 8)));
}

bool check(bool value, const char* message) {
    if (!value) std::cerr << "FAIL: " << message << '\n';
    return value;
}
}  // namespace

int main() {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() /
        ("w3d-locale-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "DBFilesClient");

    std::string strings(1, '\0');
    const uint32_t fallbackOffset = (uint32_t)strings.size();
    strings += "Fallback";
    strings.push_back('\0');
    const uint32_t mxOffset = (uint32_t)strings.size();
    strings += "Español MX";
    strings.push_back('\0');

    std::vector<uint8_t> dbc{'W', 'D', 'B', 'C'};
    appendU32(dbc, 2);                  // records
    appendU32(dbc, 16);                 // one 16-column LocalizedString
    appendU32(dbc, 16 * 4);             // record size
    appendU32(dbc, (uint32_t)strings.size());
    for (uint32_t row = 0; row < 2; ++row) {
        for (uint32_t column = 0; column < 16; ++column) {
            const uint32_t offset = column == 2 ? fallbackOffset : row == 0 && column == 7 ? mxOffset : 0;
            appendU32(dbc, offset);
        }
    }
    dbc.insert(dbc.end(), strings.begin(), strings.end());
    {
        std::ofstream file(root / "DBFilesClient" / "LocaleTest.dbc", std::ios::binary);
        file.write((const char*)dbc.data(), (std::streamsize)dbc.size());
        if (!file) return 2;
    }

    w3d::setGameLocale("esMX");
    bool ok = check(w3d::gameLocaleIndex() == 7, "esMX maps to locale index 7");
    {
        w3d::DataDir data;
        w3d::DbcTable table;
        const bool opened = data.open(root.string());
        const bool loaded = opened && table.load(data, "LocaleTest.dbc");
        ok = check(loaded, "synthetic localized DBC loads") && ok;
        if (loaded) {
            ok = check(table.loc(0, 0) == "Español MX", "selected esMX column is returned") && ok;
            ok = check(table.loc(1, 0) == "Fallback", "empty esMX column falls back to first non-empty locale") && ok;
        }
    }
    fs::remove_all(root);
    return ok ? 0 : 1;
}
