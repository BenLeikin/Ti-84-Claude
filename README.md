# TI-84 Claude Bridge

A hardware bridge that lets a Texas Instruments TI-84 Plus graphing calculator
talk to Anthropic's Claude API over WiFi, using a Seeed XIAO ESP32-S3 hidden
inside the calculator case.

The calculator becomes a front-end for an LLM: type a prompt on the keypad,
press Enter, see Claude's response on the screen. No external hardware visible,
no USB cables, just a calculator that has unexpectedly become much smarter.

## Status

Work in progress.

| Component | Status |
|-----------|--------|
| ESP32 firmware: WiFi, captive portal, API client | Working |
| Multi-network and multi-profile support | Working |
| Web configuration UI | Working |
| Power: LiPo + USB-C charging | Working |
| Internal mounting inside TI-84 case | Working |
| Link port protocol (ESP32 side) | Working |
| Calculator-side program | In Progress |

## Hardware

| Item | Notes |
|------|-------|
| Seeed XIAO ESP32-S3 | The bridge. 21x17.5mm, dual-core 240MHz, WiFi, USB-C, integrated LiPo charging |
| LiPo cell, 250mAh | Powers the ESP32 independently of the calculator's batteries |
| IPEX 2.4GHz antenna | External antenna for reliable WiFi inside the metal-LCD-and-PCB sandwich |
| TI-84 Plus (original, not CE) | Z80 at 15MHz, 96x64 mono display, runs on 4x AAA |
| 2.5mm I/O port | The calculator's link port, where the ESP32 connects internally |

A complete bill of materials with quantities, voltage ratings, and sourcing
notes will be added in `docs/parts-list.pdf`.

## Architecture

```
┌─────────────────┐     I/O port      ┌──────────────────┐      HTTPS       ┌──────────────────────┐
│  TI-84 Plus     │◄─────────────────►│  XIAO ESP32-S3   │◄────────────────►│  api.anthropic.com   │
│  - BASIC/asm    │   bit-banged      │  - WiFi          │   over user's    │  Claude Messages API │
│  - 96x64 mono   │   serial          │  - HTTPS client  │   home network   │                      │
│  - User input   │                   │  - JSON parser   │                  │                      │
└─────────────────┘                   └──────────────────┘                  └──────────────────────┘
```

The ESP32 does the heavy lifting: network, TLS, JSON, configuration storage.
The calculator runs a small program that just sends prompts and renders
responses.

## Features

**End-user friendly configuration**

- No source-code editing or compilation required to use the device
- Captive portal setup: connect a phone to the device's WiFi, fill out a web
  form, done
- Once configured, runtime web UI accessible at `http://ti84claude.local`

**Multiple WiFi networks**

- Save up to 5 networks (home, work, phone hotspot, etc.)
- Auto-scans at boot and connects to the highest-priority reachable network
- Reorder priority from the web UI

**Multiple Claude profiles**

- Save up to 5 profiles, each with its own API key, model, system prompt, and
  token limit
- Switch active profile via web UI or calculator command
- Lets one device serve different use cases (work, personal, kid-friendly) or
  multiple users with their own API keys

**Self-contained**

- No external server or companion app required
- All configuration lives in ESP32 flash storage and persists across reboots
- Internal power via LiPo cell, charged through the XIAO's USB-C port

## Build and flash

This is a [PlatformIO](https://platformio.org/) project.

```bash
# Install PlatformIO CLI if you don't have it
pip install --user platformio

# From the project root
pio run --target upload     # build and flash to connected XIAO ESP32-S3
pio device monitor          # open serial monitor at 115200 baud
```

Connect the XIAO via USB-C. On Linux, your user needs to be in the `dialout`
group to access serial devices:

```bash
sudo usermod -a -G dialout $USER
# Then log out and back in
```

## First-time setup

After flashing the firmware to a new XIAO:

1. Power on the device (USB-C cable to phone charger, laptop, or LiPo when
   the build is assembled)
2. On a phone or laptop, open WiFi settings
3. Connect to the open network named `TI84-Claude-Setup`
4. A "Sign in to network" popup should appear (this is the captive portal).
   If it doesn't, open a browser and navigate to `http://192.168.4.1`
5. Add your home WiFi network and an Anthropic API key
6. Tap Save. The device reboots and connects to your home WiFi.
7. From any device on your home network, visit `http://ti84claude.local` to
   reconfigure later

Get an Anthropic API key at
[console.anthropic.com](https://console.anthropic.com). The default profile
uses Claude Haiku 4.5, which costs roughly $0.0004 per typical short exchange.

## Project layout

```
ti84-claude/
├── platformio.ini              # build configuration
├── README.md                   # this file
├── .gitignore                  # excludes build artifacts and secrets
├── docs/                       # parts lists, wiring diagrams, design notes
└── src/                        # all source code
    ├── main.cpp                # boot sequence and main loop
    ├── storage.{h,cpp}         # NVS-backed configuration storage
    ├── profile.{h,cpp}         # Claude profile data structure
    ├── wifi_network.h          # WiFi network data structure
    ├── wifi_manager.{h,cpp}    # WiFi connection management (AP and STA)
    ├── captive_portal.{h,cpp}  # web UI with DNS hijack
    ├── claude_api.{h,cpp}      # Anthropic Messages API client
    └── serial_console.{h,cpp}  # development REPL over USB serial
```

## Configuration

All configuration is stored in the ESP32's NVS (non-volatile storage). Three
ways to manage it:

**Web UI** (recommended for end users): `http://ti84claude.local` or
`http://192.168.4.1` in AP mode.

**Serial console** (for development): Open `pio device monitor` and type
`HELP` for the command list. Examples:

```
LIST                       Show all networks and profiles
NET ADD HomeWiFi password  Add a WiFi network
PROFILE ADD work           Create a new profile
PROFILE USE 1              Switch active profile
ASK <prompt>               Send a prompt to Claude (after connecting)
STATUS                     Show WiFi and system info
```

**Direct editing**: Not supported. Storage is in NVS, not on a filesystem,
so changes go through the firmware.

## Security considerations

This is a hobby project, not a hardened device. A few honest disclosures:

- The captive portal in AP mode runs over HTTP, not HTTPS. Anyone in WiFi
  range during initial setup could intercept the API key. Setup is brief and
  local, so the risk is limited, but be aware of it.
- The API key is stored in NVS unencrypted. Anyone with physical access to
  the device and the right tools could read it from flash. This is a known
  trade-off for ease of development.
- The Claude API call uses `setInsecure()` to skip TLS certificate validation.
  In practice this still encrypts traffic, but it doesn't verify you're really
  talking to Anthropic. A MITM with a fake certificate could intercept your
  API key. For a home network, this is generally acceptable.

For a production device, all three of these would need to be addressed.

## License

Not yet decided. Treat as "all rights reserved" for now.

## Acknowledgments

Built by Ben, with substantial pair-programming and architectural help from
Claude (Anthropic). The recursive aspect of using an LLM to build a calculator
front-end for that same LLM is acknowledged and embraced.
