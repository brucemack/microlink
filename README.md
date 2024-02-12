# Overview

Is it possible to build a full EchoLink&reg; station on a $4 microcontroller?  I'm not completely sure, but
let's find out. The goal of this project is to create the smallest, simplest, 
cheapest way to put a radio onto the EchoLink network. Are you new to EchoLink?  Please 
[see the official website](https://www.echolink.org/) for complete information. 

There are much easier ways to get onto EchoLink. The MicroLink project is only
interesting for someone who wants to get deep into the nuts-and-bolts of EchoLink/VoIP technology. In fact, you 
should start to question the sanity of anyone who spends this much time building their own EL station. I am a homebrew enthusiast and I find the use of off-the-shelf software/components to be less exciting. 

Here's the current demo video:

[![MicroLink RX Demo](https://img.youtube.com/vi/wqWCYG_9o4k/0.jpg)](https://www.youtube.com/watch?v=wqWCYG_9o4k)

The microphone/analog section still needs work.  

Once things are working smoothly I will be integrating this onto a single PCB for 
ease of integration with radios (link mode) and/or repeaters.

The official 
PC-based EchoLink client written by Jonathan Taylor (K1RFD) is and excellent
product and is the quickest/easiest way to get on 
EchoLink. [Download it here](https://www.echolink.org/download.htm). There are also versions that run on mobile phones. MicroLink is not a supported part of the EchoLink product suite.

# Architecture/Parts

My goal was to build a complete station from scratch, with no strings attached to PCs/servers.  

At the moment there is no radio integration, but the final MicroLink product will provide an inexpensive interface between the internet and a radio to make linking very simple. 

This project required an in-depth examination of how the the EchoLink protocol works. [My analysis can be found here](https://github.com/brucemack/microlink/blob/main/docs/el_supplement.md).

## Current Parts List (HW)

* The main processor is a Pi Pico (RP2040) development board.  $4.00 on DigiKey.
* Internet connectivity currently comes from an ESP-32-WROOM development board. $5.00 on Amazon. Work
is underway to provide a 3G cellular data option using a SIM7600 module.
* Microphone is an electret condenser with a LVM321 pre-amp and low-pass anti-aliasing 
filter.  The microphone part needs work.
* Audio input sampling uses the integrated ADC in the RP2040.
* Audio output generation uses the MicroChip MCP4725 I2C digital-to-analog converter.  $1.27 on DigiKey.
* Audio amplification uses the LM4862M 825mW amplifier.  $2.23 on DigiKey.
* The T/R key is from Federal Telephone and Telegraph Company (Buffalo, NY), made in 1920.  Priceless.

## Current Parts List (SW)

* The main station firmware is completely homebrew (C++, see GitHub repo).
* The ESP-32 runs the Espresif AT firmware (version 3.2.0.0).
* Importantly, audio compression/decompression uses a GSM 06-10 Full Rate CODEC which is homebrew 
in C++. Getting that to work required studying
the European Telecommunications Standards Institute specification for GSM and a lot of testing,
but this was extremely interesting.

Here's a picture of the parts on the bench so you can tell what you're looking at.

![MicroLink Station](docs/demo-1.png)

## Speeds and Feeds

* The standard audio sample rate for EchoLink is 8 kHz at 12-bits.
* The audio CODEC creates/consumes one 640 byte packet every 80ms.  One of these packets is moved 12.5 times per second.
* It takes the RP2040 about 75uS to decode a 160 byte GSM frame.
* The UDP data rate needed to sustain audio quality is 
approximately 14,000 baud.
* The RP2040 runs at 125 MHz.  Only one of the two processors is used at this time.
* The DAC runs on an I2C bus running at 400 kHz.
* The ESP-32 is on a serial port that runs at 115,200 baud.

# Test Notes

## client-test-2p

This is the most comprehensive demonstration that targets the RP2040. This is 
the code shown in the video demonstration.

At the moment this uses a serial console to take commands and display 
status.

## client-test-2

* Set environment variables EL_CALLSIGN, EL_PASSWORD, EL_FULLNAME, EL_LOCATION.

# Technical/Development Notes

## Regarding Audio Smoothness

One of the most difficult challenges I had with this project was getting audio
that was "smooth" in both directions. This stuff is probably obvious to people
who are versed in the state-of-the-art of VoIP, but it's all new to me.  Here
are a few points that will help anyone getting into this.

1. Accurate/consistent clocking of the audio chain is essential.  GSM uses
an 8 kHz clock, which means we need an audio sample once every
125 microseconds. I know this seems hard to beleive, but inconsistencies
in this clock can be "heard."  This is where hardware timers with efficient
interrupt service architectures are important. The highest priority activity
of the microcontroller should be the creation/consumption of an audio sample
every 125 uS *exactly* - everything else in the system has some leeway.
2. An EchoLink receiver gets an audio packet *approximately* every 80ms.  Each 
packet contains
4 frames that each represent exactly 20 ms of audio. Finally, each frame contains 160
samples which each represent exactly 125 uS of audio.  From point #1 above, we already 
know that the timing of the 160 samples within each frame is critical.  However, 
we also need ensure that the 20 ms frames are played continuously 
*without the slighest gap between them.* This gets into an interesting problem
because the frames are streaming across the busy internet (not to mention 
low-cost WIFI hardware) and may be subject to 
small timing inconsistencies. There is simply no way to ensure that an EchoLink packet
will arrive every 80 ms. Sometimes the gap might be 81 ms, sometimes 79 ms, etc. 
This variability is known as "jitter" and it is a common issue in VoIP systems.
The fix is simple: we need to delay/buffer the audio generation in the receiver 
slighly to give ourselves a margin of error to accumulate packets. The
MicroLink system keeps a buffer of 16 audio packets and holds back the start 
of audio generation (after sqelch break) until the buffer is half full.  This 
means that the receive path is delayed by 8 x 80 ms *above and beyond* any delay
in the internet itself. Experimential work is ongoing to make this delay adaptive
so that the delay is minimized.  Of course if the buffer emplies out (i.e. several
slow packets in a row), all bets are off.

## Building Tests on Windows (CYGWIN)

(These notes are not comprehensive yet.)

    git clone https://github.com/brucemack/microlink.git
    cd microlink
    git submodule update --remote
    mkdir build
    cd build
    cmake ..
    make <target>

## Building on PI PICO

(These notes are not comprehensive yet.)

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

Pi PICO Stuff
* [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
* Analysis of ADC: https://pico-adc.markomo.me/

Rig Integration
* Microphone Connector Reference: https://www.secradio.org.za/zs6src/secfiles/pdf/mic_soc_info.pdf
