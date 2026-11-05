#include "actions.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

void check(bool value, const char* label) {
    if (!value) {
        std::fprintf(stderr, "FAILED: %s\n", label);
        std::exit(1);
    }
}

int main() {
    using namespace Doors;
    auto all = [](const char*) { return true; };
    auto none = [](const char*) { return false; };
    auto toggles = [](const char* name) { return std::strstr(name, ":toggle") != nullptr; };
    State initial;
    auto explode = plan(Explode, initial, all);
    check(explode.count == 1 && !std::strcmp(explode.names[0], "CarEvents:Explode"), "default explode");
    auto implode = plan(Explode, explode.next, all);
    check(implode.count == 1 && !std::strcmp(implode.names[0], "CarEvents:Implode"), "second activation implodes");
    check(!implode.next.exploded, "implode resets state");
    auto doors = plan(AllDoors, initial, all);
    check(doors.count == 4, "all four doors open");
    for (int i = 0; i < 4; ++i) check(!std::strcmp(doors.names[i], Open[i]), "explicit door open");
    auto closed = plan(AllDoors, doors.next, all);
    for (int i = 0; i < 4; ++i) check(!std::strcmp(closed.names[i], Close[i]), "explicit door close");
    auto two_door = [](const char* name) {
        return std::strstr(name, "doorLF:") || std::strstr(name, "doorRF:");
    };
    check(plan(AllDoors, initial, two_door).count == 2, "two-door cars skip missing rear doors");
    auto toggled = plan(AllDoors, initial, toggles);
    check(toggled.count == 4, "toggle-only graphs support all doors");
    auto toggled_back = plan(AllDoors, toggled.next, toggles);
    check(!toggled_back.next.open[0] && toggled_back.count == 4, "toggle-only doors close");
    for (int i = Hood; i < ActionCount; ++i) {
        auto action = static_cast<Action>(i);
        auto first = plan(action, initial, toggles);
        auto second = plan(action, first.next, toggles);
        check(first.count == 1 && second.count == 1, "all individual parts toggle");
        check(!std::strcmp(first.names[0], second.names[0]), "same native toggle on each press");
        check(second.next.open == initial.open, "individual toggle state returns");
        auto explicit_first = plan(action, initial, [](const char* name) {
            return std::strstr(name, ":toggle") == nullptr;
        });
        check(explicit_first.count == 1 && std::strstr(explicit_first.names[0], ":open"), "open/close fallback");
    }
    State one_open = plan(DoorLF, initial, toggles).next;
    auto mixed = plan(AllDoors, one_open, toggles);
    check(mixed.count == 3 && mixed.next.open[0] && mixed.next.open[3], "group opens only closed toggle-only doors");
    check(plan(AllDoors, explode.next, all).names[0] == Close[0], "group closes after explode");
    for (int i = 0; i < ActionCount; ++i)
        check(plan(static_cast<Action>(i), initial, none).count == 0, "unsupported action does not dispatch");
    Edge edge;
    check(!edge.sample(true, true), "held button on new car does not fire");
    check(!edge.sample(true), "held button does not repeat");
    check(!edge.sample(false) && edge.sample(true), "release then press fires once");
    check(!edge.sample(true, true) && !edge.sample(true), "binding change while held does not fire");
    std::puts("All action, capability, shared door state and edge-trigger checks passed.");
}
