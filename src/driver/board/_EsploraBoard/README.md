# EsploraBoard

Stub [`IBoard`](../../io/board/IBoard.h) for the retired [Arduino Esplora](https://docs.arduino.cc/retired/boards/arduino-esplora/).

**PoC goal:** libusb + USB HID interrupts via **`libesplora_hid`** to keep input latency low. Firmware side: Arduino-LUFA in [`Esplora-firmware`](https://github.com/pierre-quelin/Esplora-firmware).

## Fixed PCB resources

Created in the board constructor (not configurable via `Inputs` / `Item` lists):

| Instance | Type | Interface |
|----------|------|-----------|
| `switch1` … `switch4` | `driver::board::EsploraSwitch` | `IIn` |
| `rgbLed` | `driver::board::EsploraRGBLed` | `IRGBWLed` (WHITE ignored / 0) |
| `lightSensor` | `driver::board::EsploraLightSensor` | `IADC` |

Obtain paths: `EsploraBoard.switch1`, `EsploraBoard.rgbLed`, `EsploraBoard.lightSensor`.

`getOutput` / `getLed` / `getPWM` return `nullptr`.

Current state: **HID pump** via `libesplora_hid` when a matching USB device is present (`init` soft-fails to Ready without pump if none). Event-driven IN (switch change / OUT sync); pump runs on a dedicated `tools::os::thread::Thread` (`EsploraHid`) blocking with timeout 0 — not Shared EventScheduler. Firmware: [`Esplora-firmware`](https://github.com/pierre-quelin/Esplora-firmware).

USB IDs: **VID `0x1209`** / **PID `0xE5F1`** (same as firmware / `libesplora_hid`).

## Linux udev (libusb access)

Without a udev rule, `countDevices()` may succeed while `openByIndex` fails (`Ready without pump`). Install:

```bash
sudo cp src/driver/board/_EsploraBoard/_linux/99-esplora-hid.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
sudo usermod -aG plugdev "$USER"   # if needed, then re-login
```

Unplug/replug the Esplora. Rule file: [`_linux/99-esplora-hid.rules`](_linux/99-esplora-hid.rules).

## Minimal config

```json
{
  "GlobalObjects": [
    { "Item": "EsploraBoard" }
  ],
  "EsploraBoard": {
    "InstanceOf": "driver::board::EsploraBoard",
    "LogLevel": "DEBUG"
  }
}
```

## Objects overlay (Bridged / Ghost)

Optional `Objects.<name>` does **not** choose which IO exist; it only overlays creation props after `resolvePlatform`:

- No `InstanceOf` → `createSharedNamed<EsploraXxx>` (C++ type fixed); `"Bridged"` attaches a sidecar
- With `InstanceOf` (e.g. `io::in::InGhost`) → `createShared` at the same path (Remote)

### Local / Remote (Sample-style)

Two platform-specific `GlobalObjects` lists (like Sample): **Local** boots the board; **Remote** boots only the Ghost(s) at the same instance path(s).

```json
{
  "GlobalObjects": {
    "Platform": {
      "Local": [
        { "Item": "EsploraBoard" }
      ],
      "Remote": [
        { "Item": "EsploraBoard.switch1" }
      ]
    }
  },
  "EventBus": {
    "InstanceOf": "tools::design::ipc::EventBus",
    "AppName": "EsploraPoC",
    "Platform": {
      "Local": {
        "Broker": "tcp://127.0.0.1:1883"
      },
      "Remote": {
        "Broker": "tcp://127.0.0.1:1883",
        "ExpectedPlatforms": [ "Local" ],
        "SynchronizeTimeout": "30s"
      }
    }
  },
  "EsploraBoard": {
    "Platform": {
      "Local": {
        "InstanceOf": "driver::board::EsploraBoard",
        "LogLevel": "DEBUG"
      }
    },
    "Objects": {
      "switch1": {
        "Platform": {
          "Local": {
            "Bridged": "io::in::InBridge"
          },
          "Remote": {
            "InstanceOf": "io::in::InGhost"
          }
        }
      }
    }
  }
}
```

- `GlobalObjects.Platform.Local` → `EsploraBoard` (`Platform.Local.InstanceOf`)
- `GlobalObjects.Platform.Remote` → `EsploraBoard.switch1` (Ghost), same bus path as Local Bridged
- Select platform with `platformName=Local` / `Remote` in `main.ini`
- MQTT `ClientId` = `AppName` + `-` + `platformName` when absent (inline `Broker` or nested `Transport`)
- Reference config (source only, **not** installed by Sample CMake): sibling Sample repo `cfg/esplora/` (`main.json` + `local|remote/main.ini`)

## Next steps

- Joystick axes, accelerometer, mic, slider, TFT (out of current stub)
