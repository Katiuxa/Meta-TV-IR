# Meta TV IR

Pocket infrared remote for the M5Stack Cardputer ADV. Version 1.0.3.

Point the Cardputer at the television. Search walks brands until the set reacts, then hands you the remote. On / Off keeps sending power until you stop.

## Device

Cardputer ADV (Stamp-S3A, ESP32-S3, 8 MB flash, no PSRAM). This image is not for the original Cardputer.

## M5Burner

Choose `firmware/MetaTVIR-1.0.3.bin`. It is the application image (ESP32 magic `E9`), version **1.0.3**, device **Cardputer ADV**. Sound is off until turned on with Ctrl+M. The remote opens with POWER marked.

## Build

```
pio run -e cardputer-adv
```

The image is `.pio/build/cardputer-adv/firmware.bin`.
