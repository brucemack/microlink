# Overview

Is is possible to run a full EchoLink&reg; station on a microcontroller?  I'm not completely sure, but
let's find out. The goal of this project is to create the smallest,
cheapest possible way to put a radio onto the EchoLink network. Are you new to EchoLink&reg;?  Please 
[see the official website](https://www.echolink.org/) for complete information.

Here's the current demo:

[![MicroLink RX Demo](https://img.youtube.com/vi/rONuahJaLdY/0.jpg)](https://www.youtube.com/watch?v=rONuahJaLdY)

# Architecture/Parts

The goal is to build a complete station from scratch, with no strings attached to PCs/servers.  The official 
PC-based EchoLink client written by Jonathan Taylor (K1RFD) is great and is the quickest/easiest way to get on 
EchoLink. This project is only interesting for someone who wants to get into the nuts-and-bolts of EchoLink/VoIP technology. The final product will provide an inexpensive interface between the internet
and a radio to make linking very simple.

## Current Parts List (HW)

* The main processor is a Pi Pico (RP2040) development board.  $4.00 on DigiKey.
* Internet connectivity currently comes from an ESP-32-WROOM development board. $5.00 on Amazon. Work
is underway to provide a 3G cellular data option using a SIM7600 module.
* Audio input sampling uses the integrated ADC in the RP2040.
* Audio output generation uses the MicroChip MCP4725 I2C digital-to-analog converter.  $1.27 on DigiKey.
* Audio amplification uses the LM4862M 825mW amplifier.  $2.23 on DigiKey.

## Current Parts List (SW)

* The main station firmware is completely homebrew (see GitHub repo).
* The ESP-32 runs the Espresif AT firmware (version 3.2.0.0).
* Importantly, the GSM 06-10 Full Rate CODEC is homebrew.

# Test Notes

## client-test-2p

This is the most comprehensive demonstration that targets the RP2040.

## client-test-2

* Set environment variable EL_PASSWORD with password.

# Technical/Development Notes

## Building Tests on Windows (CYGWIN)

git clone https://github.com/brucemack/microlink.git
cd microlink
git submodule update --remote
mkdir build
cd build
cmake ..
make <target>

## Building on PI PICO

git clone https://github.com/brucemack/microlink.git
cd microlink
git submodule update --remote
mkdir build
cd build
cmake -DTARGET_GROUP=pico ..
make <target>

## ESP32 AT Firmware Notes

    esptool.py --chip auto --port /dev/ttyUSB0 --baud 115200 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size 4MB 0x0 /home/bruce/Downloads/ESP32-WROOM-32-V3.2.0.0/factory/factory_WROOM-32.bin

## Test Commands

    # Used to receive UDP packets
    netcat -u -l 5198
    # Used to send UDP packets.  The printf command supports non-printable.
    printf 'Hello\rWorld' | nc -u -w1 192.168.8.210 5198

References
==========

* Official EchoLink: https://www.echolink.org/
* ESP-32 AT Command Reference: https://docs.espressif.com/projects/esp-at/en/latest/esp32/AT_Command_Set/index.html
* SIM7600 module AT Command Reference: https://www.waveshare.net/w/upload/6/68/SIM7500_SIM7600_Series_AT_Command_Manual_V2.00.pdf
* SIM7600 module application notes: https://www.waveshare.com/w/upload/4/4b/A7600_Series_TCPIP_Applicati0n_Note_V1.00.pdf
* Windows Audio Related: 
  - http://www.techmind.org/wave/
  - http://midi.teragonaudio.com/tech/lowaud.htm
  - http://soundfile.sapp.org/doc/WaveFormat/
* Components
  - [MCP4725 DAC](https://ww1.microchip.com/downloads/en/devicedoc/22039d.pdf)
  - [Audio Amp](https://www.ti.com/lit/ds/symlink/lm4862.pdf?HQS=dis-dk-null-digikeymode-dsf-pf-null-wwe&ts=1707335785542&ref_url=https%253A%252F%252Fwww.ti.com%252Fgeneral%252Fdocs%252Fsuppproductinfo.tsp%253FdistId%253D10%2526gotoUrl%253Dhttps%253A%252F%252Fwww.ti.com%252Flit%252Fgpn%252Flm4862)
  - Timers: https://vanhunteradams.com/Pico/TimerIRQ/SPI_DDS.html
