Overview
========

Building Tests on Windows
=========================

git clone https://github.com/brucemack/microlink.git
cd microlink
git submodule update --remote
mkdir build
cd build
cmake ..
make <target>

Building on PI PICO
===================

mkdir build
cd build
cmake -DTARGET_GROUP=pico ..
make <target>

# Test Notes

## client-test-2

* Set environment variable EL_PASSWORD with password.

ESP32 AT Firmware
=================

    esptool.py --chip auto --port /dev/ttyUSB0 --baud 115200 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size 4MB 0x0 /home/bruce/Downloads/ESP32-WROOM-32-V3.2.0.0/factory/factory_WROOM-32.bin

# WIFI Commands

AT+CWQAP
OK
AT+CWJAP="Gloucester Island Municipal WIFI","xxx","74:83:c2:bd:df:a3"

AT+CIPSTATE?

# UDP/IP Commands

AT+CIPSTA?
+CWSTA:ip:"xxx.xxx.xxx.xxx"
...
OK

AT+CIPMUX=1
OK

AT+CIPMODE?
+CIPMODE:0

AT+CIPRECVMODE?
OK

AT+CIPRECVMODE=0
OK

AT+CIPSTART=0,"UDP","192.168.8.102",5198,5198,0
0,CONNECT
OK

AT+CIPCLOSE=0
OK

AT+CIPSEND=0,4,"192.168.8.102",5198
OK
(This greater-than sign is sent)
>
(Nothing is echoed, as soon as the length is reached we get the 'Recv ..' prompt)
Recv 4 bytes

Example of receiving:
+IPD,0,11:Hello World



Test Commands
-------------
    # Used to receive UDP packets
    netcat -u -l 5198
    # Used to send UDP packets.  The printf command supports non-printable.
    printf 'Hello\rWorld' | nc -u -w1 192.168.8.210 5198



References
==========

* https://www.echolink.org/
* AT Command Reference for SIM7600 module: https://www.waveshare.net/w/upload/6/68/SIM7500_SIM7600_Series_AT_Command_Manual_V2.00.pdf
* Application notes for cellular module: https://www.waveshare.com/w/upload/4/4b/A7600_Series_TCPIP_Applicati0n_Note_V1.00.pdf
* Windows Audio Related: 
  - http://www.techmind.org/wave/
  - http://midi.teragonaudio.com/tech/lowaud.htm
  - http://soundfile.sapp.org/doc/WaveFormat/
  

