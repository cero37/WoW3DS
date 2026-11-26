// Direct MPQ reading through StormLib, so the converter can take the client's
// Data/ folder as-is instead of a pre-extracted copy.
#include "conv.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>

#ifdef W3D_WITH_STORMLIB
#include <StormLib.h>
#endif

namespace fs = std::filesystem;

namespace w3d {

namespace {
std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// WotLK 3.3.5a archives are opened from highest to lowest priority because
// readMpq() returns the first matching file.
int mpqPriority(const fs::path& p, bool inLocaleDir) {
    std::string n = lower(p.filename().string());
    if (n.rfind("patch", 0) == 0) {
        int level = 1;
        const size_t dash = n.rfind('-');
        if (dash != std::string::npos && dash + 1 < n.size() - 4) {
            const std::string suffix = n.substr(dash + 1, n.size() - dash - 5);
            if (!suffix.empty() && std::all_of(suffix.begin(), suffix.end(), [](unsigned char c) { return std::isdigit(c); }))
                level = std::stoi(suffix);
        }
        return 1000 + level * 10 + (inLocaleDir ? 1 : 0);
    }

    struct Archive { const char* prefix; int priority; };
    static constexpr Archive archives[] = {
        {"lichking-speech-", 650}, {"expansion-speech-", 640}, {"lichking-locale-", 630},
        {"expansion-locale-", 620}, {"speech-", 610}, {"locale-", 600},
        {"lichking.mpq", 400}, {"expansion.mpq", 300}, {"common-2.mpq", 200}, {"common.mpq", 100},
    };
    for (const Archive& archive : archives)
        if (n.rfind(archive.prefix, 0) == 0)
            return archive.priority + (inLocaleDir ? 1 : 0);
    return -1;
}
}  // namespace

DataDir::~DataDir() {
#ifdef W3D_WITH_STORMLIB
    for (void* h : archives_) SFileCloseArchive((HANDLE)h);
#endif
}

bool DataDir::openMpq(const std::string& dir) {
#ifdef W3D_WITH_STORMLIB
    std::vector<std::pair<int, fs::path>> found;
    std::vector<fs::path> localeDirs;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(dir, ec)) {
        if (e.is_regular_file(ec) && lower(e.path().extension().string()) == ".mpq") {
            const int priority = mpqPriority(e.path(), false);
            if (priority >= 0) found.push_back({priority, e.path()});
        }
        else if (e.is_directory(ec) && e.path().filename().string().size() == 4)  // enUS, ruRU, ...
            localeDirs.push_back(e.path());
    }
    // 7.14: --locale prefers Data/<locale>/; without it (auto) every locale archive is read as before. A client that lacks
    // the asked folder falls back to enUS, so the base data still converts.
    const std::string want = lower(gameLocaleDir());
    auto addLocale = [&](const fs::path& d) {
        for (auto& l : fs::directory_iterator(d, ec))
            if (l.is_regular_file(ec) && lower(l.path().extension().string()) == ".mpq") {
                const int priority = mpqPriority(l.path(), true);
                if (priority >= 0) found.push_back({priority, l.path()});
            }
    };
    bool anyLocale = false;
    for (const fs::path& d : localeDirs) {
        const bool match = want.empty() || want == "auto" || lower(d.filename().string()) == want;
        if (match) { addLocale(d); anyLocale = true; }
    }
    if (!anyLocale && !want.empty() && want != "auto") {   // no such folder (enGB-only client, an extracted copy): read them all
        std::printf("  locale: Data/%s not found, using every locale archive\n", gameLocaleDir().c_str());
        for (const fs::path& d : localeDirs) addLocale(d);
    }
    if (!want.empty() && want != "auto")
        std::printf("  locale: %s (DBC text column %d)\n", gameLocaleDir().c_str(), gameLocaleIndex());
    std::stable_sort(found.begin(), found.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first > b.first;
        std::string left = lower(a.second.generic_string()), right = lower(b.second.generic_string());
        return left < right;
    });
    for (auto& [pr, path] : found) {
        HANDLE h = nullptr;
        if (SFileOpenArchive(path.string().c_str(), 0, STREAM_FLAG_READ_ONLY, &h)) {
            archives_.push_back(h);
            std::printf("  mpq [%4d] %s\n", pr, path.filename().string().c_str());
        } else {
            std::fprintf(stderr, "  cannot open %s (error %u)\n", path.string().c_str(), (unsigned)SErrGetLastError());
        }
    }
    return !archives_.empty();
#else
    (void)dir;
    std::fprintf(stderr, "this build has no MPQ support (configure with -DW3D_STORMLIB=ON)\n");
    return false;
#endif
}

bool DataDir::readMpq(const std::string& gamePath, std::vector<uint8_t>& out) const {
#ifdef W3D_WITH_STORMLIB
    std::string name = gamePath;
    std::replace(name.begin(), name.end(), '/', '\\');
    for (void* a : archives_) {
        HANDLE f = nullptr;
        if (!SFileOpenFileEx((HANDLE)a, name.c_str(), SFILE_OPEN_FROM_MPQ, &f)) continue;
        DWORD hi = 0, size = SFileGetFileSize(f, &hi);
        bool ok = false;
        if (size != SFILE_INVALID_SIZE && hi == 0) {
            out.resize(size);
            DWORD got = 0;
            ok = SFileReadFile(f, out.data(), size, &got, nullptr) || got == size;
            ok = ok && got == size;
        }
        SFileCloseFile(f);
        if (ok) return true;  // highest-priority archive that has the file wins
    }
#else
    (void)gamePath; (void)out;
#endif
    return false;
}

}  // namespace w3d
