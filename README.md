# What is this
This project implements a USB to Amiga interface that runs on the rp2040 / pico
Keyboard can be plugged into the usb port, and will appear as an Amiga keyboard
supports A500 and A2000 which are similar but subtly different
Uses the rp2040 PIO to drive the control lines, rather than bit banging

# references
## interface spec
https://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0172.html

# How to get set up
## Install pico sdk
plugin does this for you now
## vscode plugins
install c, cmake plugins
install pico plugin (official, from pi foundation)
import project using pico icon

# How to build

# How to test
## using pioemu to emulate the pio blocks
https://github.com/NathanY3G/rp2040-pio-emulator

setup virtual env using vscode
source .venv/bin/activate
pip install adafruit-circuitpython-pioasm (to compile pio code to binary for use in pioemu)
pip install rp2040-pio-emulator

# How to download to pico
## USB
## serial

# hardware
## 5 pin DIN pinout
1 - KCLK
2 - KDAT
3 - NC
4 - GND
5 - +5v