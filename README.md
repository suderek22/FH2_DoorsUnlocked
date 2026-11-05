# FH2 DoorsUnlocked 1.0.0

By [suderek](https://github.com/suderek22).

Toggle the active player car's animations with one configurable controller button.
Settings are available on the FH2 Mod Platform's in-game Mods page. Left/right steps
through named choices; changes apply immediately and are saved beside the DLL.

## Requirements

- FH2 Mod Platform **2.8.0 or newer** (API 9 named-choice settings).
- An XInput controller. Keyboard-only input is not implemented by this mod.
- A car with the corresponding animation states. Not every car has opening parts,
  active aero or an explode animation.


## Installation

1. Close the game.
2. Extract the release ZIP into your Azimuth runtime folder. Its `fh2-mods` folder
   contains `DoorsUnlocked.dll` and the default `DoorsUnlocked.ini`.
3. If upgrading, replace the DLL. Keep your existing INI to preserve 1.0.0 settings;
   the legacy `DoorsEnabled` setting from 0.1.x is no longer used.
4. Start the game, load a car, then open the mod platform's Mods settings.
5. Select **Action** and **Mapped button**. Defaults are **Explode** and **LS**.
6. Press and release the selected button to toggle. Holding it does not repeat.


## Actions

| Choice | First activation / next activation |
|---|---|
| Explode | `CarEvents:Explode` / `CarEvents:Implode` |
| Doors | Open all supported doors / close all supported doors |
| Hood | Toggle hood |
| Trunk | Toggle trunk |
| Aero | Toggle active aero |
| Door LF | Toggle left-front door |
| Door LR | Toggle left-rear door |
| Door RF | Toggle right-front door |
| Door RR | Toggle right-rear door |

Individual parts use the native `:toggle` state when available, otherwise their
`:open`/`:close` states. Doors acts on the supported doors only, so two-door cars
do not require rear-door animations. Toggle-only door graphs are supported too.
Missing states are logged instead of sending unsupported animation requests.

The first request prepares the full-detail animation branch for about 350 ms.
An additional press during preparation is reported and ignored. The full-detail
branch remains selected while that player car is active, which can increase rendering cost.
Switching cars or animation graphs resets the mod's tracked toggle state.
Changing bindings while a button is held requires releasing it before activation.

## Buttons

**LS** (left-stick click), **RS**, **LB**, **RB**, **A**, **B**, **X**, **Y**,
**D-pad Up/Down/Left/Right**, **Back**, **Start**, **LT**, **RT**.

Any connected XInput controller can activate the binding. Triggers activate above
the standard XInput trigger threshold. This mod does **not** consume or remap the
button's normal game action. Choose a binding that does not conflict with your
controls or other mods. There is no hold-to-open mode.

## INI settings

```ini
[DoorsUnlocked]
Action=0
Button=0
```

- Action: `0` Explode, `1` Doors, `2` Hood, `3` Trunk, `4` Aero,
  `5` Door LF, `6` Door LR, `7` Door RF, `8` Door RR.
- Button: `0` LS, `1` RS, `2` LB, `3` RB, `4` A, `5` B, `6` X, `7` Y,
  `8` D-pad Up, `9` Down, `10` Left, `11` Right, `12` Back, `13` Start,
  `14` LT, `15` RT.

Invalid saved indices are logged and reset to the default. The runtime folder
must be writable to persist settings.

## Compatibility and limitations

- Visual results depend on the car's assets and native animation graph.
- Automatic game animations, damage, or another animation mod can change a part's
  state independently; the mod tracks its own requests, not every external change.
- Avoid operating two mods on the same part simultaneously. Studio's animation
  commands and this mod share the native vehicle hook through the platform, but
  simultaneous animation requests can still conflict.
- The default LS binding may also trigger a normal game function. Input is not
  suppressed, including while a menu is open with an active player car.
- Installation and compiled-DLL settings/action tests are automated; visual checks
  of every car/action/controller combination still require in-game testing.
- No gameplay restrictions, vehicle physics or save data are patched.
