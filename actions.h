#pragma once
#include <array>

namespace Doors {
enum Action { Explode, AllDoors, Hood, Trunk, Aero, DoorLF, DoorLR, DoorRF, DoorRR, ActionCount };
inline constexpr const char* Names[] = {
    "Explode", "Doors", "Hood", "Trunk", "Aero", "Door LF", "Door LR", "Door RF", "Door RR"
};
inline constexpr const char* Open[] = {
    "doorLF:open", "doorLR:open", "doorRF:open", "doorRR:open",
    "hood:open", "trunk:open", "active_aero:open"
};
inline constexpr const char* Close[] = {
    "doorLF:close", "doorLR:close", "doorRF:close", "doorRR:close",
    "hood:close", "trunk:close", "active_aero:close"
};
inline constexpr const char* Toggle[] = {
    "doorLF:toggle", "doorLR:toggle", "doorRF:toggle", "doorRR:toggle",
    "hood:toggle", "trunk:toggle", "active_aero:toggle"
};
struct State {
    bool exploded = false;
    std::array<bool, 7> open{};
};
struct Plan {
    std::array<const char*, 4> names{};
    int count = 0;
    State next;
};
struct Edge {
    bool down = false;
    bool sample(bool value, bool reset = false) {
        bool pressed = !reset && value && !down;
        down = value;
        return pressed;
    }
};

template<class HasState>
Plan plan(Action action, State state, HasState has) {
    Plan result{{}, 0, state};
    if (action < 0 || action >= ActionCount) return result;
    if (action == Explode) {
        const char* name = state.exploded ? "CarEvents:Implode" : "CarEvents:Explode";
        if (has(name)) {
            result.names[result.count++] = name;
            result.next.exploded = !state.exploded;
            result.next.open.fill(result.next.exploded);
        }
    } else if (action == AllDoors) {
        bool all_open = true;
        int supported = 0;
        std::array<bool, 4> available{};
        for (int i = 0; i < 4; ++i) {
            available[i] = (has(Open[i]) && has(Close[i])) || has(Toggle[i]);
            if (available[i]) {
                all_open &= state.open[i];
                ++supported;
            }
        }
        if (!supported) return result;
        for (int i = 0; i < 4; ++i) {
            if (available[i]) {
                const char* explicit_state = all_open ? Close[i] : Open[i];
                if (has(explicit_state)) result.names[result.count++] = explicit_state;
                else if (state.open[i] == all_open) result.names[result.count++] = Toggle[i];
                result.next.open[i] = !all_open;
            }
        }
    } else {
        int index = action == Hood ? 4 : action == Trunk ? 5 : action == Aero ? 6 : action - DoorLF;
        const char* name = has(Toggle[index]) ? Toggle[index] : state.open[index] ? Close[index] : Open[index];
        if (has(name)) {
            result.names[result.count++] = name;
            result.next.open[index] = !state.open[index];
        }
    }
    return result;
}
}
