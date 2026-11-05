#include "fh2mod.h"
#include "actions.h"
#include <windows.h>
#include <xinput.h>

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <string>
#include <thread>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace {
constexpr uintptr_t kFrameRva = 0x2A9400;
constexpr uintptr_t kPlayStateRva = 0xF5D300;
constexpr uintptr_t kDetailRva = 0x16FBE90;
const FH2ModApi* g_api = nullptr;
FH2Mod* g_mod = nullptr;
std::filesystem::path g_ini;
std::atomic<int> g_action{0}, g_button{0};
std::atomic<unsigned> g_revision{0};
unsigned char* g_game = nullptr;
using FrameFn = void (*)(void*, float, float, uint32_t);
FrameFn g_frame = nullptr;

constexpr const char* kButtons[] = {
    "LS", "RS", "LB", "RB", "A", "B", "X", "Y",
    "D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right",
    "Back", "Start", "LT", "RT"
};
constexpr WORD kMasks[] = {
    XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
    XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER,
    XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
    XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN,
    XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT,
    XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_START
};

void logf(const char* fmt, ...) {
    char text[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof text, fmt, args);
    va_end(args);
    g_api->log(g_mod, text);
}

bool persist(const wchar_t* key, int value) {
    wchar_t text[24];
    swprintf_s(text, L"%d", value);
    if (WritePrivateProfileStringW(L"DoorsUnlocked", key, text, g_ini.c_str())) return true;
    logf("ERROR saving setting (Win32 %lu). Check write access to DoorsUnlocked.ini.", GetLastError());
    return false;
}

void changed(void*, const char* key, int value) {
    if (!key) return;
    if (!strcmp(key, "Action") && value >= 0 && value < Doors::ActionCount) {
        g_action = value;
        persist(L"Action", value);
    } else if (!strcmp(key, "Button") && value >= 0 && value < static_cast<int>(std::size(kButtons))) {
        g_button = value;
        persist(L"Button", value);
    } else {
        logf("ERROR invalid setting %s=%d.", key, value);
        return;
    }
    ++g_revision;
    logf("Setting %s=%d; binding applies immediately.", key, value);
}

void* player_car() {
    __try {
        void* holder = *reinterpret_cast<void**>(g_game + 0x34ABA60);
        if (!holder) return nullptr;
        void* resolved = reinterpret_cast<void*(*)(void*, void*)>(g_game + 0x21E0C0)(holder, nullptr);
        if (!resolved) return nullptr;
        void* car = nullptr;
        reinterpret_cast<void(*)(void*, void**)>(g_game + 0x2DA060)(resolved, &car);
        return car && *reinterpret_cast<void**>(car) == g_game + 0x2C5CB80 ? car : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static bool reported = false;
        if (!reported) logf("ERROR player-car lookup faulted; waiting for a valid player.");
        reported = true;
        return nullptr;
    }
}

void* animation_graph(void* car) {
    __try {
        auto controller = *reinterpret_cast<char**>(static_cast<char*>(car) + 0x4D40);
        return controller ? *reinterpret_cast<void**>(controller + 0x1C8) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        logf("ERROR reading the player's animation controller.");
        return nullptr;
    }
}

bool has_state(void* graph, const char* name) {
    __try {
        auto head = *reinterpret_cast<uintptr_t*>(static_cast<char*>(graph) + 0x268);
        if (!head) return false;
        auto node = *reinterpret_cast<uintptr_t*>(head);
        for (int i = 0; node != head && i < 256; ++i) {
            size_t length = *reinterpret_cast<size_t*>(node + 0x20);
            size_t capacity = *reinterpret_cast<size_t*>(node + 0x28);
            if (!length || length > 127) {
                logf("ERROR invalid animation graph state name.");
                return false;
            }
            const char* text = capacity < 16 ? reinterpret_cast<char*>(node + 0x10) :
                *reinterpret_cast<char**>(node + 0x10);
            if (length == strlen(name) && !memcmp(text, name, length)) return true;
            node = *reinterpret_cast<uintptr_t*>(node);
        }
        if (node != head) logf("ERROR animation graph exceeds 256 states.");
        return false;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        logf("ERROR reading animation graph states.");
        return false;
    }
}

bool full_detail(void* graph) {
    __try {
        if (*reinterpret_cast<int*>(static_cast<char*>(graph) + 0x10) != 0)
            reinterpret_cast<void(*)(void*, int)>(g_game + kDetailRva)(graph, 0);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        logf("ERROR preparing the full-detail animation branch.");
        return false;
    }
}

bool supported_build() {
    __try {
        const unsigned char play[] = {0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0x89,0x40,0x4D,0x00};
        const unsigned char detail[] = {0x8B,0xC2,0xC5,0xE8,0x57,0xD2,0x89,0x51,0x10};
        const unsigned char holder[] = {0x48,0x8B,0x49,0x48,0x48,0x85,0xC9};
        const unsigned char car[] = {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10};
        return !memcmp(g_game + kPlayStateRva, play, sizeof play) &&
            !memcmp(g_game + kDetailRva, detail, sizeof detail) &&
            !memcmp(g_game + 0x21E0C0, holder, sizeof holder) &&
            !memcmp(g_game + 0x2DA060, car, sizeof car);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool play(void* car, const char* state) {
    __try {
        reinterpret_cast<void(*)(void*, const char*)>(g_game + kPlayStateRva)(car, state);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        logf("ERROR animation call faulted: %s.", state);
        return false;
    }
}

bool button_down(int button) {
    bool down = false;
    for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
        XINPUT_STATE state{};
        DWORD result = XInputGetState(i, &state);
        if (result == ERROR_DEVICE_NOT_CONNECTED) continue;
        if (result != ERROR_SUCCESS) {
            static DWORD previous = ERROR_SUCCESS;
            if (previous != result) logf("ERROR XInputGetState: %lu.", result);
            previous = result;
            continue;
        }
        if (button < static_cast<int>(std::size(kMasks)))
            down |= (state.Gamepad.wButtons & kMasks[button]) != 0;
        else
            down |= (button == 14 ? state.Gamepad.bLeftTrigger : state.Gamepad.bRightTrigger) >
                XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    }
    return down;
}

// All game access and animation dispatch run on the native vehicle-update thread.
void frame(void* car, float dt, float frame_dt, uint32_t tick) {
    g_frame(car, dt, frame_dt, tick);
    if (car != player_car()) return;
    static void* current = nullptr;
    static void* current_graph = nullptr;
    static Doors::State state;
    static Doors::Edge edge;
    static unsigned revision = 0;
    static int pending = -1;
    static ULONGLONG prepared_at = 0;
    static bool retain_detail = false;
    void* graph = animation_graph(car);
    bool reset = current != car || current_graph != graph;
    if (reset) {
        current = car;
        current_graph = graph;
        state = {};
        pending = -1;
        retain_detail = false;
    }
    unsigned latest = g_revision.load();
    bool trigger = edge.sample(button_down(g_button.load()), reset || revision != latest);
    revision = latest;
    if (retain_detail && graph && !full_detail(graph)) {
        retain_detail = false;
        pending = -1;
    }
    if (trigger) {
        if (!graph) logf("Action unavailable: the active car has no animation graph.");
        else if (pending >= 0) logf("Action ignored: an animation is still preparing.");
        else if (full_detail(graph)) {
            pending = g_action.load();
            prepared_at = GetTickCount64();
            retain_detail = true;
        }
    }
    if (pending < 0 || GetTickCount64() - prepared_at < 350) return;
    auto action = static_cast<Doors::Action>(pending);
    pending = -1;
    auto plan = Doors::plan(action, state, [graph](const char* name) { return has_state(graph, name); });
    if (!plan.count) {
        logf("Action unavailable: %s is not supported by this car's animation graph.", Doors::Names[action]);
        return;
    }
    for (int i = 0; i < plan.count; ++i) {
        if (!play(car, plan.names[i])) return;
        logf("Animation submitted: %s.", plan.names[i]);
    }
    state = plan.next;
}

void worker() {
    g_game = static_cast<unsigned char*>(g_api->wait_for_code(180000));
    if (!g_game) {
        logf("ERROR timed out waiting for game code.");
        return;
    }
    MEMORY_BASIC_INFORMATION memory{};
    if (!supported_build() || !VirtualQuery(g_game + kFrameRva, &memory, sizeof memory) ||
        !(memory.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) {
        logf("ERROR unsupported native game build or modified animation entry points; no hook installed.");
        return;
    }
    auto hook = g_api->hook_function(g_mod, "DoorsUnlocked: vehicle animations", g_game + kFrameRva,
                                    reinterpret_cast<void*>(frame), reinterpret_cast<void**>(&g_frame));
    if (!hook || !g_api->enable_hooks(g_mod, hook)) {
        logf("ERROR could not install the vehicle-update hook.");
        return;
    }
    logf("Ready. Action=%s; button=%s. Press once to toggle; release before pressing again.",
         Doors::Names[g_action.load()], kButtons[g_button.load()]);
}

bool add_choice(const char* key, const char* label, const char* description,
                const char* const* names, unsigned count, int value) {
    FH2Setting setting{};
    setting.size = sizeof setting;
    setting.key = key;
    setting.label = label;
    setting.description = description;
    setting.kind = FH2_SETTING_CHOICE;
    setting.choices = names;
    setting.choice_count = count;
    setting.value = value;
    setting.on_change = changed;
    if (g_api->add_setting(g_mod, &setting)) return true;
    logf("ERROR registering the %s selector.", label);
    return false;
}

int read_setting(const wchar_t* key, int count) {
    wchar_t text[32];
    GetPrivateProfileStringW(L"DoorsUnlocked", key, L"0", text, static_cast<DWORD>(std::size(text)), g_ini.c_str());
    wchar_t* end = nullptr;
    long value = wcstol(text, &end, 10);
    if (!*text || *end || value < 0 || value >= count) {
        logf("ERROR invalid saved %ls; restoring default 0.", key);
        return 0;
    }
    return static_cast<int>(value);
}
}

FH2MOD_EXPORT int fh2mod_init(const FH2ModApi* api) {
    if (!api || api->api_version < 9 || api->size < sizeof(FH2ModApi) ||
        !api->register_mod || !api->log || !api->add_setting || !api->wait_for_code ||
        !api->hook_function || !api->enable_hooks) return 1;
    g_api = api;
    FH2ModInfo info{};
    info.size = sizeof info;
    info.name = "DoorsUnlocked";
    info.version = "1.0.0";
    info.github_repo = "suderek22/FH2_DoorsUnlocked";
    info.platform_version = "2.8.0";
    info.check_updates = 1;
    info.description = "Toggle explode/implode, doors, hood, trunk or aero using a configurable controller button.";
    g_mod = api->register_mod(&info);
    if (!g_mod) return 1;
    wchar_t path[32768];
    DWORD length = GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), path, static_cast<DWORD>(std::size(path)));
    if (!length || length >= std::size(path)) {
        logf("ERROR resolving the mod folder (Win32 %lu).", GetLastError());
        return 1;
    }
    g_ini = std::filesystem::path(path).parent_path() / L"DoorsUnlocked.ini";
    g_action = read_setting(L"Action", Doors::ActionCount);
    g_button = read_setting(L"Button", static_cast<int>(std::size(kButtons)));
    if (!persist(L"Action", g_action.load()) || !persist(L"Button", g_button.load())) return 1;
    if (!add_choice("Action", "Action", "Left/right selects an animation. Press the mapped button once to toggle.",
                    Doors::Names, Doors::ActionCount, g_action.load()) ||
        !add_choice("Button", "Mapped button", "LS = left stick click. This does not suppress the button's normal game action.",
                    kButtons, static_cast<unsigned>(std::size(kButtons)), g_button.load())) return 1;
    std::thread(worker).detach();
    return 0;
}
