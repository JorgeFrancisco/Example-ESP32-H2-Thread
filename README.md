| Supported Targets | ESP32-H2 |
| ----------------- | -------- |

# ESP32-H2 Matter over Thread Relay

Matter **On/Off Plug-in Unit** over Thread for the ESP32-H2 that drives a relay,
with a factory reset on the BOOT button and an RGB status LED. Commissioned over
Bluetooth LE and controlled through a Thread Border Router (for example a
Tuya/MOES "Matter & Thread" gateway).

Built with ESP-IDF v5.5.5 and `espressif/esp_matter` 1.6 from the ESP Component
Registry (no esp-matter clone or `ESP_MATTER_PATH` needed).

## Features

- On/Off cluster drives a relay; the state is persisted by Matter.
- `StartUpOnOff` (Lighting feature): relay state at boot. Default: previous state.
- Commissioning over Bluetooth LE, then Thread (Full Thread Device, can act as a
  Thread router).
- Hold **BOOT for 10 s** to factory reset. Removing the device from its last
  controller also factory resets it, so it can be commissioned again.
- WS2812 status LED showing the commissioning/network state.

## Hardware

Board: **ESP32-H2-DevKitM-1** (ESP32-H2-MINI-1, 4 MB flash).

| Function   | GPIO | Notes                                  |
| ---------- | ---- | -------------------------------------- |
| Status LED | 8    | On-board addressable RGB LED (WS2812)  |
| BOOT       | 9    | On-board button, active low            |
| Relay      | 10   | Active high (`RELAY_ACTIVE_LEVEL`)     |

Pins and timings are in `main/app_config.h`.

## Status LED

| LED               | Meaning                                                   |
| ----------------- | --------------------------------------------------------- |
| Off               | Not commissioned, commissioning window closed             |
| Blue, blinking    | Waiting to be commissioned (Bluetooth LE advertising)     |
| Purple, blinking  | Commissioning in progress                                 |
| Purple, solid     | Commissioned, joining the Thread network                  |
| Green, solid      | Attached to the Thread network (first 10 s)               |
| White / off       | Connected for more than 10 s: relay on / relay off        |
| Red, solid (3 s)  | Commissioning failed, removed, or factory reset           |
| Red, blinking     | Commissioned but no Thread network for 3 minutes          |

After 10 s connected (`STATUS_LED_CONNECTED_MS`) the LED mirrors the relay
output. Any other state (commissioning, removal, errors, Thread network lost)
shows the status again; after reconnecting, green is shown for 10 s once more.

When the commissioning window closes without being commissioned (LED off), hold
BOOT for 10 s to open it again.

## Requirements

- A **Thread Border Router** that supports Matter commissioning. A Zigbee-only
  hub does not work: the device talks Thread, not Zigbee.
- A Matter controller app (Tuya/Smart Life, Google Home, Apple Home, Home
  Assistant, ...). Bluetooth must be enabled on the phone.
- Internet access on the first build (the Component Manager downloads
  `esp_matter`).

## Build and flash

### Windows (PowerShell)

With ESP-IDF v5.5.5 installed by the ESP-IDF Installation Manager (EIM) in
`C:\Espressif`. Adjust the paths and the serial port (`COM4`) to your setup.

```powershell
# 1. Allow PowerShell to run scripts (only once per Windows user)
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned

# 2. Load the ESP-IDF environment (once per terminal window)
. 'C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1'

# 3. Short Component Manager cache (see "Windows path length" below).
#    Once per terminal window, or permanently with:
#    [Environment]::SetEnvironmentVariable('IDF_COMPONENT_CACHE_PATH', 'C:\Espressif\cc', 'User')
$env:IDF_COMPONENT_CACHE_PATH = 'C:\Espressif\cc'

# 4. Go to the project folder
cd C:\Projetos\ESP32\Example-ESP32-H2-Thread

# 5. Build (the target esp32h2 comes from sdkconfig.defaults).
#    The first build downloads esp_matter and takes a while.
idf.py build

# 6. Erase the flash on the first flash (and to forget fabrics/Thread network)
idf.py -p COM4 erase-flash

# 7. Flash and open the serial monitor (exit with Ctrl+])
idf.py -p COM4 flash monitor
```

In VS Code with the ESP-IDF extension, **"ESP-IDF: Open ESP-IDF Terminal"**
opens a terminal with the environment already loaded (steps 1 and 2 are not
needed; step 3 is, unless the variable was set permanently).

### Windows path length

The `esp_matter` package contains the Matter SDK sources with very long file
names. With Windows long paths disabled (the default), paths are limited to 260
characters:

- The default Component Manager cache (`%LOCALAPPDATA%\Espressif\ComponentManager\Cache`)
  exceeds the limit and the download fails with `FileNotFoundError` while
  extracting `esp_matter`. `IDF_COMPONENT_CACHE_PATH` moves it to a short folder.
- In `managed_components`, the longest file reaches 258 characters with the
  project in `C:\Projetos\ESP32\Example-ESP32-H2-Thread` (41 characters). A
  longer project path breaks the build.

Enabling Windows long paths removes both limits (administrator PowerShell,
then open a new terminal):

```powershell
New-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' -Name LongPathsEnabled -Value 1 -PropertyType DWORD -Force
```

### Other systems

```bash
. $HOME/esp/esp-idf/export.sh     # or your ESP-IDF activation script
idf.py build
idf.py -p <PORT> erase-flash
idf.py -p <PORT> flash monitor
```

## Commissioning

The firmware uses the Matter **test** commissioning data:

| Field          | Value                    |
| -------------- | ------------------------ |
| QR code        | `MT:Y.K9042C00KA0648G00` |
| Manual code    | `34970112332`            |
| Passcode       | `20202021`               |
| Discriminator  | `3840`                   |

These are the standard test values (VID `0xFFF1`, PID `0x8000`, Bluetooth LE);
they are not printed in the serial log. To show the QR code, open
`https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT:Y.K9042C00KA0648G00`.

1. Power the board: the LED blinks blue.
2. In the app, add a **Matter** device and scan the QR code (or type the manual
   code). Choose the home where the Thread Border Router is.
3. Tuya-based apps (Tuya/Smart Life, Nova Digital) may ask for the Wi-Fi SSID
   and password: skip that step and tell the app the device does not use Wi-Fi
   (the ESP32-H2 has no Wi-Fi radio).
4. With test credentials the app may warn that the device is **not certified**;
   if it offers to continue, accept. Apps that refuse uncertified devices cannot
   commission this firmware.
5. The LED blinks purple while commissioning, then turns green once the device
   is attached to the Thread network. The device shows up as a plug/outlet and
   the relay follows the app's on/off. The first commands right after adding it
   may not reach the device while it is still settling in the Thread network.

Deleting the device in the app removes its fabric; with no fabric left the
device factory resets and blinks blue again.

### Home Assistant first, then Tuya (multi-admin)

A Matter device can be controlled by several controllers at the same time, each
with its own fabric. To use Home Assistant as the main controller and still see
the device in the Tuya app:

1. In the Home Assistant **Matter Server** app configuration, enable
   `enable_test_net_dcl` and restart it. Without it Home Assistant rejects the
   test certificates and aborts right after device attestation.
2. Make sure the phone knows the Thread network credentials (Home Assistant
   Thread integration, preferred network synced to the Companion app).
3. Factory reset the board (hold BOOT for 10 s) and commission it from the
   Home Assistant Companion app: **Settings > Connectivity > Matter > Add
   device > "No, it's new"**, then scan the QR code. It can take a few minutes:
   the phone commissions the device first and then hands it over to Home
   Assistant (two fabrics appear in the log).
4. In Home Assistant, use the device's **share** option to open a commissioning
   window and get a temporary pairing code.
5. In the Tuya app, add a **Matter** device with that code (not the board's QR
   code), skip the Wi-Fi step and accept the uncertified device warning.

Removing the device from one controller only removes that controller's fabric;
the device factory resets only when the last fabric is removed. See
"Home Assistant and Tuya hubs" below before changing the power-on behavior.

## Implementation notes

### Why Matter over Thread (and not Zigbee) for Tuya-based hubs

Tuya gateways only accept Zigbee sub-devices that carry a Tuya license, written
at manufacturing time to chips running the Tuya Zigbee SDK. A custom ESP32-H2
Zigbee firmware is reported as **not authorized** in the app. Tuya apps do
commission third-party Matter devices, so with a hub that includes a Thread
Border Router the ESP32-H2 can join as a standard Matter device instead.

### Home Assistant and Tuya hubs

- Every applied attribute change is logged, including writes from controllers:
  `esp_matter_attribute: ********** W : Endpoint 0x0001's Cluster 0x00000006's Attribute 0x00004003 is 1 **********`
  (`0x4003` is `StartUpOnOff`: 0 off, 1 on, 2 toggle, null previous state).
- **Change the power-on behavior only from Home Assistant commissioned directly.**
  When the setting goes through a Tuya hub (for example Home Assistant
  controlling the device via the hub), the hub writes `StartUpOnOff` as `null`
  whatever is selected, so the device always restores its previous state and
  any value set before is overwritten. On/off commands are forwarded correctly.
- With Home Assistant commissioned directly (see "Home Assistant first, then
  Tuya"), the real value is written (`... Attribute 0x00004003 is 0` from the
  Home Assistant fabric). With the Tuya app added afterwards through sharing,
  the hub did not rewrite `StartUpOnOff` by itself during our tests.
- Direct commissioning requires `enable_test_net_dcl` in the Matter Server app
  configuration (otherwise Home Assistant aborts right after device attestation,
  because of the test certificates) and the Thread credentials on the phone.

### Test credentials

`CONFIG_ENABLE_TEST_SETUP_PARAMS` and the default device attestation use the
Matter test vendor (VID `0xFFF1`, PID `0x8000`). They are fine for development,
but controllers show the device as uncertified and some may refuse it. Real
products need their own factory data (`esp-matter-mfg-tool`) and Matter
certification.

### Data model

The device is created with the esp-matter data model (`on_off_plug_in_unit`):
endpoint 0 is the Root Node and endpoint 1 the relay (Identify, Groups, Scenes
Management, On/Off with the Lighting feature). Unused clusters are disabled in
`sdkconfig.defaults` to save flash and RAM.

## Project structure

| File                   | Responsibility                                               |
| ---------------------- | ------------------------------------------------------------ |
| `main/app_main.cpp`    | Matter node, commissioning events, status LED state          |
| `main/app_config.h`    | Configuration (pins, timings)                                |
| `main/relay.c`         | Relay output                                                 |
| `main/reset_button.c`  | BOOT button (factory reset)                                  |
| `main/status_led.c`    | WS2812 status LED                                            |
| `sdkconfig.defaults`   | Target, Bluetooth LE, OpenThread, Matter options and clusters |
| `partitions.csv`       | 4 MB layout: two OTA slots and Matter factory partitions     |

## Known warnings

- `the choice symbol SEC_CERT_DAC_PROVIDER ... is defined with a prompt outside the choice`
  (and `Symbol SEC_CERT_DAC_PROVIDER defined in multiple locations`): the symbol is
  declared both in the `esp_matter` component and in the Matter SDK Kconfig
  shipped inside it. Harmless.
- The first build compiles about 1900 files; later builds are incremental. The
  application uses about 1.7 MB of the 1.875 MB OTA slots.
