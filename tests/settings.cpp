#include "fh2mod.h"
#include <windows.h>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static FH2ModInfo info;
static std::vector<FH2Setting> settings;
static HANDLE worker_finished;
static void check(bool value, const char* label) {
    if (!value) {
        std::fprintf(stderr, "FAILED: %s (Win32 %lu)\n", label, GetLastError());
        std::exit(1);
    }
}
static FH2Mod* reg(const FH2ModInfo* value) {
    info = *value;
    return reinterpret_cast<FH2Mod*>(1);
}
static void logger(FH2Mod*, const char* text) {
    if (std::strstr(text, "Timed out") || std::strstr(text, "timed out")) SetEvent(worker_finished);
}
static int add(FH2Mod*, const FH2Setting* value) {
    settings.push_back(*value);
    return 1;
}
static void* wait_code(uint32_t) { return nullptr; }
static FH2Hook* hook(FH2Mod*, const char*, void*, void*, void**) { return nullptr; }
static int enable(FH2Mod*, FH2Hook*) { return 0; }

int main(int argc, char** argv) {
    check(argc == 3 || argc == 4, "DLL and isolated test folder supplied");
    bool saved = argc == 4 && !std::strcmp(argv[3], "saved");
    bool invalid = argc == 4 && !std::strcmp(argv[3], "invalid");
    auto directory = std::filesystem::absolute(argv[2]);
    std::filesystem::create_directories(directory);
    auto dll = directory / "DoorsUnlocked.dll";
    auto ini = directory / "DoorsUnlocked.ini";
    std::filesystem::copy_file(argv[1], dll, std::filesystem::copy_options::overwrite_existing);
    if (std::filesystem::exists(ini)) std::filesystem::remove(ini);
    if (saved || invalid) {
        check(WritePrivateProfileStringW(L"DoorsUnlocked", L"Action", saved ? L"8" : L"-1", ini.c_str()) != 0,
              "write fixture action");
        check(WritePrivateProfileStringW(L"DoorsUnlocked", L"Button", saved ? L"15" : L"99", ini.c_str()) != 0,
              "write fixture button");
        check(WritePrivateProfileStringW(L"DoorsUnlocked", L"DoorsEnabled", L"0", ini.c_str()) != 0,
              "write legacy setting");
    }
    worker_finished = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    check(worker_finished != nullptr, "worker event");
    auto module = LoadLibraryW(dll.c_str());
    check(module != nullptr, "load compiled mod");
    auto init = reinterpret_cast<int(*)(const FH2ModApi*)>(GetProcAddress(module, "fh2mod_init"));
    check(init != nullptr, "mod init export");
    FH2ModApi api{};
    api.size = sizeof api;
    api.api_version = 8;
    check(init(&api) == 1, "reject platform without named choices");
    api.api_version = 9;
    api.register_mod = reg;
    api.log = logger;
    api.add_setting = add;
    api.wait_for_code = wait_code;
    api.hook_function = hook;
    api.enable_hooks = enable;
    check(init(&api) == 0, "initialize mod");
    check(!std::strcmp(info.version, "1.0.0"), "mod version 1.0.0");
    check(info.check_updates == 1 && !std::strcmp(info.github_repo, "suderek22/FH2_DoorsUnlocked"),
          "platform-managed GitHub update checks");
    check(settings.size() == 2, "exactly action and button selectors");
    check(settings[0].kind == FH2_SETTING_CHOICE && settings[0].choice_count == 9, "nine named action choices");
    check(!std::strcmp(settings[0].choices[0], "Explode") && settings[0].value == (saved ? 8 : 0), "load action/default Explode");
    check(settings[1].kind == FH2_SETTING_CHOICE && settings[1].choice_count == 16, "sixteen button choices");
    check(!std::strcmp(settings[1].choices[0], "LS") && settings[1].value == (saved ? 15 : 0), "load button/default LS");
    for (auto& setting : settings) check(!setting.needs_restart && setting.on_change, "live settings callbacks");
    settings[0].on_change(nullptr, "Action", 8);
    settings[1].on_change(nullptr, "Button", 15);
    check(GetPrivateProfileIntW(L"DoorsUnlocked", L"Action", -1, ini.c_str()) == 8, "persist action");
    check(GetPrivateProfileIntW(L"DoorsUnlocked", L"Button", -1, ini.c_str()) == 15, "persist button");
    settings[0].on_change(nullptr, "Action", 9);
    settings[1].on_change(nullptr, "Button", -1);
    check(GetPrivateProfileIntW(L"DoorsUnlocked", L"Action", -1, ini.c_str()) == 8, "reject invalid action");
    check(GetPrivateProfileIntW(L"DoorsUnlocked", L"Button", -1, ini.c_str()) == 15, "reject invalid button");
    check(WaitForSingleObject(worker_finished, 5000) == WAIT_OBJECT_0, "worker reports unavailable game code");
    std::puts("Compiled DLL settings, defaults, persistence, validation and update registration passed.");
}
