# Smart E-Ink Air Quality Monitor
This repository contains the ESPHome software configurations and 3D printed parts to build the Desktop Air Quality Monitor featured on [the channel](https://www.youtube.com/@FeatureNAB). 

> **This fork** builds the full variant **without a custom PCB**: it uses the off-the-shelf [Seeed Studio XIAO ePaper Display Board EE05](https://wiki.seeedstudio.com/epaper_ee05/) (XIAO ESP32-S3 Plus + e-paper driver) and a **Good Display GDEY0426T82** 4.26" 800x480 e-paper panel instead of the custom driver PCB and the 2.13" panel.

[![Watch the video](https://img.youtube.com/vi/DqiMmY5ppnE/0.jpg)](https://www.youtube.com/watch?v=DqiMmY5ppnE)

The device uses an ESP32 and the Sensirion SEN66 to track 9 different air quality metrics (PM1, PM2.5, PM4, PM10, VOCs, NOx, CO2, Temperature, and Humidity) and displays them on an e-ink screen. It connects natively to [Home Assistant](https://www.home-assistant.io/) and the firmware is written using [ESPHome](https://esphome.io/).

The original (2.13" + custom PCB) 3D files can be found on [Printables](https://www.printables.com/model/1751003-smart-desktop-air-quality-monitor-esp32-sen66), on [Makerworld](https://makerworld.com/en/models/2918992-smart-desktop-air-quality-monitor-esp32-sen66), or in the releases tab.
  
## What's in this Repository?

**If you just want the final ready-to-go files, check the [Releases tab](../../releases) on the right!**

All source files are organized into their respective folders: 

*   📂 **`enclosure/ee05_sen66_pod/`** - A SEN66 side-pod that bolts onto an EE05 + 4.26" enclosure, and the script that generates it.
*   📂 **`firmware/`** - ESPHome `.yaml` configuration files, plus precompiled bin files that you can flash directly to your board (Minimal and Home Assistant versions available).

## Getting Started

This project comes in 2 flavours - the minimal "just get the sensor working" variant, and the full variant with an e-paper display and a 3D printed housing.

### Minimal variant 

#### 1. Hardware
To build this version, you will need:

*   [Sensirion SEN66](https://mou.sr/44c63Ji)
*   [Seeed XIAO ESP32-C6](https://www.youtube.com/redirect?event=video_description&redir_token=QUFFLUhqbEVLbTd0dGJnZFpQeENEZVg1UGV1QXZnaUF5UXxBQ3Jtc0tuU0hGTnIzSEkyQ1NUTmktUU1jV1ZqeVBKUG5LbHlEQ3ZrbjM0MkF5VjBqczRQbVNVd1lCc0IwV2NaUm5vb1FXYU1ZSzBDdEpOTm85MUpHb0VqLWpDdVdZSXR3NWdzQ0hybEtKcmFlbVRkcVdJc2p5RQ&q=https%3A%2F%2Fwww.seeedstudio.com%2FSeeed-StudioXIAO-ESP32C6-3PCS-p-5918.html%3Fsensecap_affiliate%3DdzMqXsY%26referring_service%3Dlink&v=DqiMmY5ppnE) microcontroller
*   [A JST-GH to Dupont cable](https://www.tinytronics.nl/en/cables-and-connectors/cables-and-adapters/jst-compatible/jst-ghr-06v-s-to-dupont-female-compatible-cable-6p-15cm) to connect them

Assemble by connecting the 3V, GND, SDA and SCL pins as shown in the video.

#### 2. Firmware

**Method A: "As Fast As Possible" (No Coding Required)**
1. If you plan to use the device with Home Assistant, download `Firmware_minimal_with_Home_Assistant_bins.zip `, if you plan to use it standalone use `Firmware_minimal_air_sensor_bins.zip ` from the Releases tab on the right side of this page.
2. Plug your ESP32 (C3, C5, C6, or S3) into your computer via USB.
3. Using a Web Serial compatible browser (like Chrome or Edge), go to [web.esphome.io](https://web.esphome.io).
4. Click "Connect", select your board's COM port, and upload the `.bin` file that matches your specific ESP32 model.
5. Connect to the device's fallback Wi-Fi network to add it to your home network!

**Method B: Do this if you already have Home Assistant (or want to customise your setup)**
1. Open the `Software` folder and grab the relevant `.yaml` config file. Copy its contents.
2. Paste it into your ESPHome Builder dashboard.
3. Edit your Wi-Fi credentials, encryption keys, and anything else you want to try.
4. "Install" to compile and flash directly to your board.

---

### Full Variant (EE05 + 4.26" e-paper, no custom PCB)

![Display layout preview](firmware/The%20Everything%20Bagel/layout_preview.png)
![Trend page preview](firmware/The%20Everything%20Bagel/layout_preview_trend.png)

#### 1. Parts

| Part | Notes |
| --- | --- |
| [Sensirion SEN66](https://sensirion.com/products/catalog/SEN66) | The air quality sensor |
| JST GH 1.25 mm 6-pin cable, single-ended (plug on one end, bare wires on the other), 10–15 cm | SEN66 to EE05. Some SEN66 sellers include one |
| [Seeed Studio XIAO ePaper Display Board EE05](https://wiki.seeedstudio.com/epaper_ee05/) | XIAO ESP32-S3 Plus and 24-pin e-paper driver on one board. Replaces the custom PCB **and** the separate XIAO |
| [Good Display GDEY0426T82](https://buyepaper.com/products/gdey0426t82) | 4.26" 800x480 black/white e-paper (SSD1677), 24-pin FPC. Plugs straight into the EE05 |
| USB-C cable + 5 V supply | The SEN66's fan runs continuously, so run it from USB rather than a battery |
| EE05 + 4.26" enclosure (e.g. the "Cattt Casing") + the [SEN66 side-pod](enclosure/ee05_sen66_pod/) | Optional. Hardware: 4× M3 and 4× M2 heat-set inserts (or none), 2× M3×8, 2× M3×10, 4× M2 screws; details in the pod README |
| *Only if needed:* 2x 10 kΩ resistors and a little heat-shrink | I²C pull-ups, if the sensor doesn't work reliably on the ESP32's internal pull-ups (see step 3 below) |

#### 2. Wiring

1. **Display.** Flip up the latch on the EE05's 24-pin connector, slide the GDEY0426T82's ribbon in straight with its gold contacts facing up (away from the board; Seeed's schematic lists it as a top-contact connector), and close the latch.
2. **Find pin 1 on the sensor cable.** Plug the cable into the SEN66. With the connector side facing you and the round fan housing on your right, pin 1 is at the left end. Wire colours aren't standardised, so go by position.
3. **Wires 5 and 6** are tied inside the sensor to GND and VDD. Cut them short and cover the ends with heat-shrink.
4. **Solder the other four into the EE05's pads.** Strip about 3 mm of each wire and solder it into its hole. The pads are the two rows of holes along the long edges beside the XIAO, labelled on the back of the board. If you're using the side-pod, feed the wires through the back plate's slot first.

| SEN66 pin | Signal | EE05 pad |
| --- | --- | --- |
| 1 | VDD | **3V3** |
| 2 | GND | **GND** |
| 3 | SDA | **D4** (GPIO5) |
| 4 | SCL | **D5** (GPIO6) |
| 5, 6 | GND / VDD | not connected, insulated |

*   **Don't use the 5V pad** next to GND. The SEN66 is a 3.3 V part (3.6 V absolute maximum).
*   **3V3 only has power while the firmware is running.** The EE05 switches the e-paper and the 3V3 pad with D6 / GPIO43, and the firmware turns it on at boot, so don't use D6 for anything else.
*   **Other pins in use:** the e-paper uses D3 (BUSY), D7 (CS), D8 (SCK), D10 (MOSI), D11 (RST) and D16 (DC). Key1 to Key3 are on D1, D2 and D9, and D0/D12 handle battery sensing. Check the pad names against the silkscreen and the [EE05 schematic](https://files.seeedstudio.com/wiki/Epaper/EE05/XIAO_ePaper_Display_Board_Ex05_V1.0.pdf) before soldering.

#### 3. Test, and add pull-ups only if needed

Sensirion's datasheet asks for 10 kΩ pull-up resistors on SDA and SCL. This build starts without them and relies on the ESP32's weak internal pull-ups, which usually work with a short cable at 100 kHz. (The original project's minimal variant runs the same way.)

Flash the firmware and watch the logs for a few minutes. If you see `Found i2c device at address 0x6B` and readings arrive every 30 s with no I²C or `sen6x` errors, you're done. If the sensor isn't found, or readings drop out, add two 10 kΩ pull-up resistors, spliced into the cable: one from wire 3 (SDA) to wire 1 (VDD), one from wire 4 (SCL) to wire 1. Slide heat-shrink on first, then cut each wire, twist the ends together with the resistor leg, solder, and shrink.

#### 4. Enclosure

Any EE05 + 4.26" enclosure works as long as the SEN66's inlets and outlet can reach room air. For the "Cattt Casing" (EE05 frame / back plate / back cap / stand), print the [SEN66 side-pod](enclosure/ee05_sen66_pod/) and make one cable hole in the back plate. Details are in that folder's README.

#### 5. Firmware
The config is `firmware/The Everything Bagel/air_sensor_epaper_ee05.yaml`. Compile it yourself with ESPHome Builder or the ESPHome CLI (Method B above), after copying `aqi_algo.h`, `display_renderer.h`, `device_extras.h` and the three `.png` icons next to it. It needs a recent ESPHome (it uses the `epaper_spi` display platform; tested with 2026.6).

**Secrets.** Put these in your `secrets.yaml` (the one in the repo only has placeholders; don't commit real values): `wifi_ssid`, `wifi_password`, `api_encryption_key`, `ota_password`, `fallback_ap_password`, `purpleair_api_key`, `purpleair_sensor_index`, `purpleair_backup_sensor_index` (optional), `airnow_api_key` and `airnow_zip`.

**Settings** at the top of the config (`substitutions:`):
*   `use_fahrenheit`: show °F on the display. Home Assistant still gets °C and converts it itself.
*   `temperature_offset`: if the case makes the sensor read warm, e.g. `"-1.5"`. Humidity is recalculated to match, so it stays correct.
*   `timezone`: for the clock.

**Standalone or with Home Assistant.** The device doesn't need Home Assistant. The clock uses internet time (SNTP), outdoor air quality comes straight from PurpleAir or AirNow, and the automatic reboot ESPHome normally does after 15 minutes without a Home Assistant connection is turned off. If Home Assistant is connected, it gets every reading, the advice text, and indoor and outdoor AQI sensors.

**Outdoor air quality.** The device uses outdoor PM2.5 to decide whether airing out is a good idea, and shows the outdoor AQI in the header. It has two sources:
*   **PurpleAir** (first choice): a nearby outdoor community sensor, fetched every 10 minutes and adjusted with the US EPA correction for PurpleAir sensors. Readings are skipped if the sensor is indoor, hasn't reported for 30 minutes, or its two laser counters disagree. You need a read API key from [develop.purpleair.com](https://develop.purpleair.com/) and the sensor's number, which is the `select=` value in its link on the PurpleAir map. You can also set a backup sensor, which is only asked when the main one has no usable reading, so it costs nothing extra on normal days. PurpleAir charges per request from a points balance: about 10 points per request, so roughly 43,000 a month. New accounts get 1,000,000 free points. Sensors that don't report humidity are corrected assuming 50%.
*   **AirNow** (fallback, US only): used when PurpleAir has no recent reading. Free API key from [docs.airnowapi.org](https://docs.airnowapi.org/), plus your ZIP code. Fetched every 30 minutes.

Either or both can be left unset; without any outdoor data that part is simply skipped. Home Assistant gets the outdoor AQI and PM2.5 in use, and which source they came from.

**The screen**
*   The main page shows the recommendation, icon and indoor US AQI on the left, and all 9 readings on the right.
*   Readings past their limit are drawn white-on-black. The limits are CO2 over 1000 ppm, PM2.5 over 25, PM10 over 45, VOC index 150+, NOx index 20+ and humidity outside 30–60%. They're all in `aqi_limits` in `aqi_algo.h`.
*   Arrows on CO2 and PM2.5 show a clear rise or fall over the last 10 minutes.
*   The header shows the date, outdoor AQI and time, plus a Wi-Fi-off icon when the connection drops.
*   If the SEN66 stops responding, the screen says so and shows dashes instead of old readings.
*   The screen only redraws when something visible changes, with a partial refresh. It does a full refresh every 30 redraws to clear ghosting.

**Keys on the side of the EE05**
*   **Key1**: redraw now.
*   **Key2**: switch to the 24-hour CO2 and PM2.5 trend page. It returns to the main page after 2 minutes. The history is kept in memory, so it starts again after a reboot.
*   **Key3**: full refresh, to clear ghosting.

If the image comes out mirrored or upside down on your panel, change `rotation:` (0/90/180/270), or add `transform: {mirror_x: false, mirror_y: false}` to the display.

**Desktop preview.** `esphome run eg_host.yaml` shows the screens with made-up readings (needs SDL2). Add `-s preview_page 1` for the trend page, or `-s use_fahrenheit true -s preview_wifi false`.

## Home Assistant Dashboard

If you are connecting this to Home Assistant and want the UI dashboard card shown in the video, you will need to install HACS (Home Assistant Community Store) and download the **[Air Quality Card by KadenThomp36](https://github.com/KadenThomp36/air-quality-card)**. The thresholds and algorithms in my `aqi_algo.h` match the WHO 2021 recommendations also used by this card, so the recommendations shown on the physical display on this Air Quality device should match those on this card/dashboard.

## Support the project
[Patreon](https://www.patreon.com/FEATURE418) for anyone interested, so I can continue making projects like this. Thanks!

Some of the links on this page are affiliate links and help support these projects at no cost to you.
