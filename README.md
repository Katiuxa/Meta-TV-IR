# META TV

**Infrared remote and local Wi-Fi TV control for the M5Stack Cardputer ADV.**

Current release: [1.1.10](https://github.com/Katiuxa/Meta-TV-IR/releases/tag/v1.1.10).

Point it at an unknown TV and sweep published brand codes until the set reacts, then lock on and drive it. On your own Wi-Fi it also listens for screens that announce themselves, opens YouTube on them, and can drive the ones that speak a network remote. The search shows Wi-Fi waves and a bar that fills, then stops. It does not call out a search, and it does not scan IP addresses.

---

## Key features

* **Auto-sweep (`A`):** Cycles brand codes. Press `A` again the moment the TV reacts and the sweep freezes on that brand.
* **Stealth probe (`Tab`):** Test with `POWER`, `MUTE`, or `VOL+`. Mute and volume find a match without turning the screen off.
* **Region (`R` on the home menu):** Europe, Asia, Americas, Japan, Oceania, or All, so the sweep is shorter.
* **Full IR remote (`G`):** Volume, channel, mute, input, and the direction pad for the locked brand.
* **ON / OFF:** Sends power codes across the brands in the selected region until you stop it.
* **Wi-Fi search:** Join a network from the device. The password is typed once and remembered on the Cardputer. Waves and a bar run for about five seconds, then the list appears.
* **Rickroll Attack / YouTube Attack:** Send the test video, or save your own. A link can be the 11-character id, or a `youtube.com`, `youtu.be`, Shorts, embed, or live address. `X` asks before a saved link is removed. `E` changes it.
* **Wi-Fi Remote:** Arrows pick a key. `OK` sends that key over Wi-Fi. **IR OK** is a separate button (`space`) for an on-screen prompt. You do not aim the Cardputer for the Wi-Fi keys.
* **Favorites (`S`):** Save a brand from the sweep for later.
* **Quiet boot:** The buzzer is off until you press `Ctrl+M`.

Wi-Fi keys work with Samsung, LG, Sony, Roku, Panasonic, and Philips. Other sets keep the infrared remote. Codes are published remote codes, not guessed ones.

---

## Controls

| Key | Action |
| :--- | :--- |
| `<` / `>` | Move |
| `OK` | Open the highlighted item, or send the Wi-Fi key |
| `space` | Infrared OK only |
| `A` | Start / freeze auto-sweep |
| `Tab` | Switch the test signal (`POWER`, `MUTE`, `VOL+`) |
| `G` | Open the infrared remote |
| `S` | Save the brand to Favorites |
| `R` | Region on the home menu. Inside NETWORK, search again |
| `W` | Pick another Wi-Fi network |
| Backtick | Back |
| `Ctrl+M` | Sound on / off |

On a found TV the four buttons are **Wi-Fi Remote**, **Rickroll Attack**, **YouTube Attack**, and **IR remote**.

Inside YouTube Attack: add a link, name it, `OK` sends it. `X` asks, then `OK` removes it. `E` changes it. The gray example under the box is `dQw4w9WgXcQ`.

---

## Hardware

* **Supported:** M5Stack Cardputer ADV (Stamp-S3A, ESP32-S3, 8 MB flash, no PSRAM).
* **Not supported:** Original Cardputer (v1.0 / v1.1). There is no infrared receiver on the ADV, so this build only transmits.

---

## Installation (M5Burner)

1. Open **M5Burner** and select **Cardputer ADV**.
2. Load `firmware/MetaTVNet-1.1.10.bin`. This is the full flash image, bootloader included, which is what M5Burner writes.
3. Flash **1.1.10**.

Flash settings match the image header: ESP32-S3, 8 MB, QIO, 80 MHz.

`firmware/MetaTVNet-1.1.10-app.bin` is only the application, for M5Launcher on a microSD card. Do not upload that file to M5Burner.

The same image is listed on [M5 Partner](https://burner.m5stack.com/G3FGBG).

The Wi-Fi password is typed on the device. `wifi.txt` in this repo is an empty optional preload and does not contain a network key.

---

## Build from source

```bash
python -m platformio run -e cardputer-adv
```

The application image is written to `.pio/build/cardputer-adv/firmware.bin`.
