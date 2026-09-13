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
- Per-board factory data: own QR code, device attestation certificate and
  vendor/product names (generated with `esp-matter-mfg-tool`).
- Hold **BOOT for 10 s** to factory reset. Removing the device from its last
  controller also factory resets it, so it can be commissioned again.
- WS2812 status LED showing the commissioning/network state, then the relay.
- Read-only REST API over Thread (`GET /api/config`): relay state and power-on
  behavior as JSON.

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
- Factory data for the board (see "Factory data"). The firmware reads the
  commissioning data from the `fctry` partition; without it the device cannot
  be commissioned.
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

# 6. Erase the flash on the first flash (and to forget fabrics/Thread network).
#    This also erases the factory data: flash it again in step 8.
idf.py -p COM4 erase-flash

# 7. Flash the firmware
idf.py -p COM4 flash

# 8. Flash the factory data of this board (see "Factory data")
python -m esptool --chip esp32h2 -p COM4 write_flash 0x3E0000 <uuid>-partition.bin

# 9. Open the serial monitor (exit with Ctrl+])
idf.py -p COM4 monitor
```

In VS Code with the ESP-IDF extension, **"ESP-IDF: Open ESP-IDF Terminal"**
opens a terminal with the environment already loaded (steps 1 and 2 are not
needed; step 3 is, unless the variable was set permanently).

After adding or removing source files in `main/`, run `idf.py reconfigure`
before building: `SRC_DIRS` only collects the sources when CMake configures.
After changing `sdkconfig.defaults`, delete `sdkconfig` so the defaults are
applied again.

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
idf.py -p <PORT> flash
python -m esptool --chip esp32h2 -p <PORT> write_flash 0x3E0000 <uuid>-partition.bin
idf.py -p <PORT> monitor
```

## Factory data (QR code, certificate and names)

Each board has its own factory data, flashed to the `fctry` partition
(`0x3E0000`, 24 KB): commissioning passcode and discriminator (the QR code),
device attestation certificate (DAC) and key, and the device information shown
by the controllers (vendor name, product name, hardware version, VID/PID,
serial number). It is generated with
[esp-matter-mfg-tool](https://github.com/espressif/esp-matter-tools/tree/main/mfg_tool),
and the QR code comes from the same generation as the data flashed to the board.

Tested on the ESP32-H2-DevKitM-1: commissioned in Home Assistant with the
generated QR code, which then showed the configured vendor and product names.

> **Never commit the generated files.** They include the DAC private key and
> the passcode verifier of the board. Keep them in private storage: they are
> needed to flash the same identity again after an erase. `out/` is ignored by
> `.gitignore` for that reason.

### 1. Install the tool (Windows workaround)

`esp-matter-mfg-tool` 1.0.25 pins `cffi==1.15.0` for Python < 3.13, which has no
prebuilt wheel for the Python 3.11 shipped with ESP-IDF, so `pip` tries to
compile it and fails without Microsoft C++ Build Tools. Install it in a separate
virtual environment (not the ESP-IDF one, to avoid changing its packages) with
the `cffi` version the tool itself uses on Python 3.13:

```powershell
$venv = "$env:USERPROFILE\mfgvenv"
& 'C:\Espressif\tools\python\python.exe' -m venv --without-pip $venv
Invoke-WebRequest -UseBasicParsing https://bootstrap.pypa.io/get-pip.py -OutFile get-pip.py
& "$venv\Scripts\python.exe" get-pip.py
& "$venv\Scripts\python.exe" -m pip install --no-deps esp-matter-mfg-tool
& "$venv\Scripts\python.exe" -m pip install "bitarray>=2.6.0" "cryptography==44.0.1" "cffi==1.17.1" `
  "future==0.18.3" "pycparser==2.21" "pypng==0.0.21" "PyQRCode==1.2.1" "python_stdnum==1.18" `
  "esp-secure-cert-tool==2.3.6" "ecdsa==0.19.0" "esp_idf_nvs_partition_gen==0.1.9" `
  "click==8.1.7" "click-option-group==0.5.7"
& "$venv\Scripts\esp-matter-mfg-tool.exe" --help
```

`pip` reports that `esp-matter-mfg-tool` requires `cffi==1.15.0`; that warning is
expected. The Python bundled with ESP-IDF has no `ensurepip`, hence
`--without-pip` and `get-pip.py`.

### 2. Get the Matter test credentials

They are not included in the `esp_matter` component. Download them from
[connectedhomeip](https://github.com/project-chip/connectedhomeip/tree/93abd8e6891bb578ea63254fb29d099936f345c8/credentials/test),
the commit used by `esp_matter` 1.6:

- `credentials/test/attestation/Chip-Test-PAI-FFF2-8001-Cert.pem`
- `credentials/test/attestation/Chip-Test-PAI-FFF2-8001-Key.pem`
- `credentials/test/certification-declaration/Chip-Test-CD-FFF2-8001.der`

The SDK only ships test Certification Declarations for vendors `0xFFF2` and
`0xFFF3`, so the board uses VID `0xFFF2`, PID `0x8001`. The DAC generated from
this test PAI is still a test certificate: controllers show the device as
uncertified and Home Assistant needs `enable_test_net_dcl`.

### 3. Generate the factory data

```powershell
& "$venv\Scripts\esp-matter-mfg-tool.exe" -v 0xFFF2 -p 0x8001 `
  --vendor-name "<vendor name>" --product-name "<product name>" `
  --hw-ver 1 --hw-ver-str v1.0 `
  --pai -k Chip-Test-PAI-FFF2-8001-Key.pem -c Chip-Test-PAI-FFF2-8001-Cert.pem `
  -cd Chip-Test-CD-FFF2-8001.der `
  --passcode <8 digits> --discriminator <0-4095> `
  --outdir <private folder>
```

- `--vendor-name`, `--product-name` and `--hw-ver-str` are what controllers show
  as vendor, product and hardware version (instead of `TEST_VENDOR`,
  `TEST_PRODUCT` and `TEST_VERSION`).
- Passcode: 8 digits from `00000001` to `99999998`, except `11111111`,
  `22222222` ... `99999999`, `12345678` and `87654321`.
- Discriminator: `0` to `4095`; use a different one for each board.
- Always pass `--outdir`: the default is the current directory.

Output in `<outdir>/fff2_8001/<uuid>/`:

| File                    | Content                                                    |
| ----------------------- | ---------------------------------------------------------- |
| `<uuid>-partition.bin`  | Factory partition to flash (24,576 bytes)                  |
| `<uuid>-qrcode.png`     | QR code                                                    |
| `<uuid>-onb_codes.csv`  | QR payload (`MT:...`), manual code, discriminator, passcode |
| `internal/`             | DAC certificate and **private key**, PAI certificate       |

The QR code can also be shown from its payload at
[project-chip.github.io/connectedhomeip/qrcode.html](https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT:Y.K9042C00KA0648G00)
by replacing the `data=` value with the board's `MT:` payload.

### 4. Firmware configuration

`sdkconfig.defaults` already selects the factory data providers:

```
CONFIG_ENABLE_TEST_SETUP_PARAMS=n
CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER=y
CONFIG_ENABLE_ESP32_DEVICE_INSTANCE_INFO_PROVIDER=y
CONFIG_FACTORY_PARTITION_DAC_PROVIDER=y
CONFIG_FACTORY_COMMISSIONABLE_DATA_PROVIDER=y
CONFIG_FACTORY_DEVICE_INSTANCE_INFO_PROVIDER=y
CONFIG_CHIP_FACTORY_NAMESPACE_PARTITION_LABEL="fctry"
```

The label must match the `fctry` partition of `partitions.csv` (the Matter SDK
default is `nvs`).

### 5. Flash and check

Flash the firmware and `<uuid>-partition.bin` at `0x3E0000` (steps 6 to 8 of
"Build and flash"). At boot the log shows the values read from the factory
partition:

```
chip[DIS]: Advertise commission parameter vendorID=65522 productID=32769 discriminator=<discriminator>/..
```

Then commission with the board's QR code or manual code.

### Going back to the Matter test codes

Replace the lines of step 4 in `sdkconfig.defaults` with
`CONFIG_ENABLE_TEST_SETUP_PARAMS=y`, delete `sdkconfig` and rebuild. The board
then uses the standard test data (QR code `MT:Y.K9042C00KA0648G00`, manual code
`34970112332`, VID `0xFFF1`, PID `0x8000`) and no factory partition is needed.

## Commissioning

1. Power the board: the LED blinks blue.
2. In the app, add a **Matter** device and scan the board's QR code (or type
   its manual code). Choose the home where the Thread Border Router is.
3. Tuya-based apps (Tuya/Smart Life, Nova Digital) may ask for the Wi-Fi SSID
   and password: skip that step and tell the app the device does not use Wi-Fi
   (the ESP32-H2 has no Wi-Fi radio).
4. With test certificates the app may warn that the device is **not
   certified**; if it offers to continue, accept. Apps that refuse uncertified
   devices cannot commission this firmware.
5. The LED blinks purple while commissioning, then turns green once the device
   is attached to the Thread network. The device shows up as a plug/outlet and
   the relay follows the app's on/off. The first commands right after adding it
   may not reach the device while it is still settling in the Thread network.

Deleting the device in the app removes its fabric; with no fabric left the
device factory resets and blinks blue again. The factory data (QR code,
certificate, names) is not affected by a factory reset, only by `erase-flash`.

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
   device > "No, it's new"**, then scan the board's QR code. It can take a few
   minutes: the phone commissions the device first and then hands it over to
   Home Assistant (two fabrics appear in the log).
4. In Home Assistant, use the device's **share** option to open a commissioning
   window and get a temporary pairing code.
5. In the Tuya app, add a **Matter** device with that code (not the board's QR
   code), skip the Wi-Fi step and accept the uncertified device warning.

Removing the device from one controller only removes that controller's fabric;
the device factory resets only when the last fabric is removed. See
"Home Assistant and Tuya hubs" below before changing the power-on behavior.

## REST API

A read-only HTTP server (`esp_http_server`, port 80) starts when the device
attaches to the Thread network:

```bash
curl -g "http://[<device IPv6 address>]/api/config"
```

```json
{"relay":{"on":false},"power_on_behavior":{"start_up_on_off":0,"mode":"off"}}
```

| Field                               | Meaning                                         |
| ----------------------------------- | ----------------------------------------------- |
| `relay.on`                          | Current relay state (OnOff attribute)           |
| `power_on_behavior.start_up_on_off` | `StartUpOnOff` value: `0`, `1`, `2` or `null`   |
| `power_on_behavior.mode`            | `off`, `on`, `toggle` or `previous` (`null`)    |

There is no authentication, so the endpoint is read-only on purpose; control
stays on Matter.

### Finding the address

Thread is IPv6 only: the device has no IPv4 address and does not show up in
the router's client list. 10 s after attaching (`REST_API_ADDRESS_LOG_DELAY_MS`)
the addresses are logged, for example:

```
rest_api: IPv6 (link-local) fe80:0000:0000:0000:b0d0:9ef9:2d5d:122c
rest_api: IPv6              fd1a:9c98:026f:99fb:2676:ef0c:9799:2261   <- mesh-local (Thread only)
rest_api: IPv6              fdeb:6781:6cda:0001:d5e3:c59b:d9f7:1ab3   <- reachable from the LAN
```

Use the address in the prefix the border router announces on the LAN, the one
the computer has a route to (on Windows: `Get-NetRoute -AddressFamily IPv6`).
Tested from a Windows PC on Wi-Fi through the Tuya hub, without any manual
route: HTTP 200 in about 0.2 s.

### Resource usage

Measured on this build (`idf.py size`, `size-components`, `size-files` and the
free heap logged when the server starts):

| Item                                                            | Cost                                            |
| --------------------------------------------------------------- | ----------------------------------------------- |
| Flash: `http_parser` + `esp_http_server` + `rest_api.cpp`       | 23.9 KB (12.3 + 10.4 + 1.1)                     |
| Flash: whole application image                                  | +42.8 KB (1,702,048 → 1,744,832 bytes)          |
| Static RAM (`.bss`/`.data`)                                     | negligible (6 bytes in `rest_api.cpp`)          |
| Heap when the server starts (task stack, control socket, state) | ~7.2 KB (42,376 → 35,212 bytes free)            |

The rest of the image growth is other code the server pulls in (most likely
the lwIP TCP/socket API, which Matter alone does not use). The server is
configured for 1 URI handler and 2 simultaneous connections to keep the heap
cost low: after Matter starts only about 35 KB of heap is left, so keep
additions small.

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

### Test certificates

The DAC in the factory data is signed by the Matter SDK **test** PAI
(`Chip-Test-PAI-FFF2-8001`) and uses test VID/PID. It is fine for development,
but controllers show the device as uncertified and some may refuse it. Real
products need a PAI from a Matter certificate authority, their own VID/PID and
Matter certification.

### Data model

The device is created with the esp-matter data model (`on_off_plug_in_unit`):
endpoint 0 is the Root Node and endpoint 1 the relay (Identify, Groups, Scenes
Management, On/Off with the Lighting feature). Unused clusters are disabled in
`sdkconfig.defaults` to save flash and RAM.

## Project structure

| File                   | Responsibility                                                          |
| ---------------------- | ----------------------------------------------------------------------- |
| `main/app_main.cpp`    | Matter node, commissioning events, status LED state                     |
| `main/app_config.h`    | Configuration (pins, timings)                                           |
| `main/relay.c`         | Relay output                                                            |
| `main/rest_api.cpp`    | Read-only REST API over Thread (IPv6)                                   |
| `main/reset_button.c`  | BOOT button (factory reset)                                             |
| `main/status_led.c`    | WS2812 status LED                                                       |
| `sdkconfig.defaults`   | Target, Bluetooth LE, OpenThread, Matter, factory data providers, clusters |
| `partitions.csv`       | 4 MB layout: two OTA slots, `fctry` factory data and secure cert partitions |

## Known warnings

- `the choice symbol SEC_CERT_DAC_PROVIDER ... is defined with a prompt outside the choice`
  (and `Symbol SEC_CERT_DAC_PROVIDER defined in multiple locations`): the symbol is
  declared both in the `esp_matter` component and in the Matter SDK Kconfig
  shipped inside it. Harmless.
- The first build compiles about 1900 files; later builds are incremental. The
  application image is about 1.74 MB, 11% of the 1,966,080-byte OTA slots is
  left.
