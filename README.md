<div align="center">

<!-- Replace with a logo or a render of the board (width 250 looks good) -->
<img width="250" alt="HexAD" src="https://github.com/user-attachments/assets/87ba5c8c-2f50-45fb-96f0-59af92693438" />

<h1>
  <b>
    HexAD
  </b>
</h1>

<h3> <b> A 12 pad USB MIDI controller with 6 rotary encoders and a TFT screen! </b> </h3>

</div>

## **Overview**

A USB MIDI controller built around a Raspberry Pi Pico (RP2040) with:

- **12 Cherry MX style pads** in a 4x3 diode matrix, sending MIDI notes
- **6 EC11 rotary encoders with push buttons**, sending MIDI CC messages
- **1.8" 128x160 TFT display** that shows which pads are held, the encoder levels and the last MIDI message
- **MCP23017 I2C expander** that reads the encoder A/B lines, because the Pico ran out of GPIOs
- Class compliant **USB MIDI**, so there are no drivers to install
- No Case required, absolutely no 3d print needed!

### Why?

I wanted a proper MIDI controller with real pads, real knobs and a screen that tells me what is going on, instead of guessing which knob does what. It also gave me a reason to design a board with a GPIO expander for the first time.

also because KNOBS.

### How?

The firmware is in [firmware](firmware). It is written with the Arduino-Pico core and TinyUSB MIDI. Core 0 scans the pads, the encoders and sends the MIDI. Core 1 only draws the display, so a screen refresh never makes the knobs lag.

The pads are plain switches, so they send a fixed velocity of 100.

#### MIDI map

| Control | Message | Notes |
| --- | --- | --- |
| Pads (bottom row, left to right) | Note 36 to 39 | Channel 1 |
| Pads (middle row) | Note 40 to 43 | Channel 1 |
| Pads (top row) | Note 44 to 47 | Channel 1 |
| Encoders 1 to 6 | CC 20 to 25 | Absolute value, starts at 64, steps of 2 |
| Encoder buttons 1 to 6 | CC 30 to 35 | 127 when pressed, 0 when released |

#### Pin map

| Function | Pins |
| --- | --- |
| Pad matrix columns | GPIO0 to GPIO3 |
| Pad matrix rows | GPIO4 to GPIO6 |
| Encoder buttons | GPIO7, 10, 11, 12, 16, 17 |
| I2C (MCP23017 at 0x27) | SDA GPIO8, SCL GPIO9 |
| TFT (SPI0) | SCK GPIO18, MOSI GPIO19, CS GPIO14, DC GPIO13, RST GPIO15 |

The MCP23017 runs on 3.3V with 4.7k pull-ups on SDA and SCL. Its address pins are pulled high with solder jumpers to ground, so you can change the I2C address without touching the firmware wiring. The spare GPB4 to GPB7 pins are broken out on a 4 pin header.

## CAD
CAD files are available at [/CAD](/CAD).

## Images
<div align="center">

<img width="912" height="655" alt="image" src="https://github.com/user-attachments/assets/4396e558-7e65-4bd1-9dd4-9574d95225d6" />

<img width="912" height="655" alt="image" src="https://github.com/user-attachments/assets/aba6c57a-8ce1-4f83-bea2-078303d5a46a" />


<br> Full render<br>

---

<img width="540" height="487" alt="image" src="https://github.com/user-attachments/assets/eedd0d81-a63e-4a2e-8c55-7d2eb03e0969" />

<br> PCB

---

<img width="779" height="485" alt="image" src="https://github.com/user-attachments/assets/c34cef87-f2d5-4f7f-a97e-30ca9c23c3d1" />

<br> Sch

---

<img width="472" alt="image" src="https://github.com/user-attachments/assets/87ba5c8c-2f50-45fb-96f0-59af92693438" />
<br> 3D Viewer

---

</div>
