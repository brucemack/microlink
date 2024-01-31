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

References
==========

* https://www.echolink.org/
* AT Command Reference for SIM7600 module: https://www.waveshare.net/w/upload/6/68/SIM7500_SIM7600_Series_AT_Command_Manual_V2.00.pdf
* Application notes for cellular module: https://www.waveshare.com/w/upload/4/4b/A7600_Series_TCPIP_Applicati0n_Note_V1.00.pdf
* Windows Audio Related: 
  - http://www.techmind.org/wave/
  - http://midi.teragonaudio.com/tech/lowaud.htm
  - http://soundfile.sapp.org/doc/WaveFormat/
  

