# META TV IR

**Interactive TV Brand Scanner & Universal Infrared Remote for M5Stack Cardputer ADV.**

Current release: [1.0.5](https://github.com/Katiuxa/Meta-TV-IR/releases/tag/v1.0.5).

Point your Cardputer at any unknown TV, sweep through regional brand databases until the screen reacts, lock onto the signal, and take full control—or use continuous brute-force mode to toggle power across nearby sets.

---

## Key Features

* **Interactive Brand Search:** Step through brands manually (`<` / `>`) or trigger **Auto-Sweep (`A`)** to rapidly fire IR bursts across popular brands. The moment the TV reacts, press `A` again to freeze the scan and lock onto that brand.
* **Region Filtering:** Narrow down the brand database by region before scanning to find matching TVs faster.
* **Stealth Signal Testing (`Tab`):** Cycle the test trigger between `POWER`, `MUTE`, and `VOL+`—allowing you to identify a TV without turning it off.
* **Instant Full Remote (`G`):** Once locked onto a brand, press `G` to open a complete remote control interface.
* **Favorites (`S`):** Save detected TVs for quick access later.
* **ON / OFF Brute-Force Mode:** Continuously spams power signals across brands until you manually stop it.
* **Silent by Default:** Audio feedback is muted on boot (`Ctrl+M` to toggle sound).

---

## Controls

| Key | Action |
| :--- | :--- |
| `<` / `>` | Previous / Next brand |
| `OK` | Fire current test signal (`POWER` selected by default) |
| `A` | Start / Freeze Auto-Sweep |
| `Tab` | Switch test signal (`POWER` -> `MUTE` -> `VOL+`) |
| `G` | Open full TV remote for the selected brand |
| `S` | Save brand to Favorites |
| `R` | Change Region filter |
| `Ctrl+M` | Toggle sound on/off |

---

## Hardware Compatibility

* **Supported:** **M5Stack Cardputer ADV** (Stamp-S3A, ESP32-S3, 8 MB Flash, no PSRAM).
* **Not Supported:** Original M5Stack Cardputer (v1.0 / v1.1).

---

## Installation (M5Burner)

1. Open **M5Burner** and select **Cardputer ADV**.
2. Search for **META TV IR** (or load `firmware/MetaTVIR-1.0.5.bin`. This is the full flash image, bootloader included, which is what M5Burner writes).
3. Flash version **1.0.5** to your device. Device: **Cardputer ADV** only.

*(M5Launcher on microSD uses the application image, `firmware/MetaTVIR-1.0.5-app.bin`, not the M5Burner file.)*

---

## Build from Source

Built with **PlatformIO**:

```bash
pio run -e cardputer-adv
```

The compiled binary is generated at:

```text
.pio/build/cardputer-adv/firmware.bin
```
