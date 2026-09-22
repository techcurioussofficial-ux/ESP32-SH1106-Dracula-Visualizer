# ESP32 SH1106 Dracula Lyrics Visualizer

Animated lyrics visualizer for **ESP32 + 7-pin SH1106 OLED (SPI)**.

Syncs words from the song with 9 different background effects and text animations.

## Features
- 9 animated backgrounds:
  - Bat Swarm
  - Geometric Mandala
  - Synthwave 3D
  - Tunnel + EQ
  - Vortex
  - Matrix Rain
  - Oscilloscope
  - Lightning
  - 3D Rotating Cube
- Text effects: Pop, Bounce, Shake, Invert, Glitch, Zoom In, Typewriter
- Auto scene switching with lyrics timeline
- Loops continuously

## Hardware
- ESP32 Dev Module
- 7-pin SH1106 128x64 OLED (SPI)

### Wiring

| OLED Pin | ESP32 Pin |
|----------|-----------|
| GND      | GND       |
| VCC      | 3.3V      |
| D0 (CLK) | 18        |
| D1 (MOSI)| 23        |
| RES      | 16        |
| DC       | 17        |
| CS       | 5         |

## Libraries Required
Install from Arduino Library Manager:
- `Adafruit GFX Library`
- `Adafruit SH110X`

## How to Upload
1. Open `DraculaVisualizer.ino` in Arduino IDE
2. Select Board: **ESP32 Dev Module**
3. Select correct COM port
4. Click Upload

## Credits
Lyrics timeline based on "Dracula" style visualizer concept.
Adapted for SH1106 SPI OLED on ESP32.
