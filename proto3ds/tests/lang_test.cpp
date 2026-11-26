// lang_test — 7.15: the interface language tables (English key -> translation), escapes and fallbacks.
#include <cstdio>
#include <cstring>
#include <chrono>
#include <filesystem>

#include "ui_style.hpp"

static int g_fail = 0, g_ok = 0;
#define CHECK(c, ...) do { if (c) ++g_ok; else { ++g_fail; std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

int main(int argc, char** argv) {
    const char* path = "lang_test_tmp.txt";
    FILE* f = fopen(path, "wb");
    if (!f) return 2;
    fputs("Bags\tBolsas\nSell \tVender \nLine\\nBreak\tLinea\\nRota\nOnly here\tSolo aqui\n", f);
    fclose(f);

    ui::g_lang = ui::kLangSpanish;
    CHECK(ui::loadLanguageTable(path), "table loaded");
    CHECK(!strcmp(ui::tr("Bags", "Сумки"), "Bolsas"), "Spanish lookup");
    CHECK(!strcmp(ui::tr("Sell ", "Продать "), "Vender "), "a trailing space is kept (got '%s')", ui::tr("Sell ", ""));
    CHECK(!strcmp(ui::tr("Line\nBreak", ""), "Linea\nRota"), "escaped newline");
    CHECK(!strcmp(ui::tr("Not translated", "fallback"), "Not translated"), "missing key falls back to English");
    CHECK(ui::trText("Bags") == "Bolsas", "trText uses the table");
    ui::setLanguage(0);
    CHECK(!strcmp(ui::tr("Bags", "Сумки"), "Bags"), "English");
    remove(path);
    if (argc > 1) {
        ui::g_lang = ui::kLangSpanish;
        CHECK(ui::loadLanguageTable(argv[1]), "the bundled Spanish file loads");
        CHECK(!strcmp(ui::tr("Bags", ""), "Bolsas"), "bundled: Bags -> Bolsas");
        CHECK(!strcmp(ui::tr("Quests", ""), "Misiones"), "bundled: Quests -> Misiones");
    }
    const std::filesystem::path oldCwd = std::filesystem::current_path();
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() /
        ("w3d-lang-alias-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(tempDir / "sdmc:/3ds/w3dproto");
    std::filesystem::current_path(tempDir);
    FILE* alias = fopen("sdmc:/3ds/w3dproto/lang_esMX.txt", "wb");
    if (!alias) return 2;
    fputs("Bags\tBolsas MX\n", alias);
    fclose(alias);
    ui::setLanguage(ui::kLangSpanish);
    CHECK(!strcmp(ui::tr("Bags", ""), "Bolsas MX"), "Spanish prefers the esMX table");
    remove("sdmc:/3ds/w3dproto/lang_esMX.txt");
    alias = fopen("sdmc:/3ds/w3dproto/lang_esES.txt", "wb");
    if (!alias) return 2;
    fputs("Bags\tBolsas ES\n", alias);
    fclose(alias);
    ui::setLanguage(0);
    ui::setLanguage(ui::kLangSpanish);
    CHECK(!strcmp(ui::tr("Bags", ""), "Bolsas ES"), "Spanish falls back to the esES table");
    std::filesystem::current_path(oldCwd);
    std::filesystem::remove_all(tempDir);
    std::printf("%d ok, %d failed\n", g_ok, g_fail);
    return g_fail ? 1 : 0;
}
