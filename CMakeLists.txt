# MicroLink EchoLink Station
# Copyright (C) 2024, Bruce MacKinnon KC1FSZ
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# ( at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# NOT FOR COMMERCIAL USE WITHOUT PERMISSION.
#
cmake_minimum_required(VERSION 3.13)
if(DEFINED ENV{PICO_BUILD})
include(pico_sdk_import.cmake)
endif()
project(microlink C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
file(MAKE_DIRECTORY tmp)

if(DEFINED ENV{PICO_BUILD})
pico_sdk_init()
endif()

if(DEFINED ENV{PICO_BUILD})
message("----- PICO build ---------------------------------------------------------------")
add_compile_options(-Wall -g)
else()
message("Desktop build!")
add_compile_options(-fstack-protector-all -Wall -Wpedantic -g)
endif()

# ------ server-test-1 -------------------------------------------------------

add_executable(server-test-1
  tests/server-test-1.cpp
  src/common.cpp
)

target_include_directories(server-test-1 PRIVATE src)

# ------ socket-test-1 -------------------------------------------------------

add_executable(socket-test-1
  tests/socket-test-1.cpp
  tests/TestContext.cpp
  src/contexts/SocketContext.cpp
  src/common.cpp
  kc1fsz-tools-cpp/src/Common.cpp
)

target_include_directories(socket-test-1 PRIVATE src)
target_include_directories(socket-test-1 PRIVATE kc1fsz-tools-cpp/include)

# ------ packet-test-1 -------------------------------------------------------

add_executable(packet-test-1
  tests/packet-test-1.cpp
  src/common.cpp
  ../gsm-0610-codec/src/fixed_math.cpp
  ../gsm-0610-codec/src/wav_util.cpp
  ../gsm-0610-codec/src/Parameters.cpp
  ../gsm-0610-codec/src/Encoder.cpp
  ../gsm-0610-codec/src/Decoder.cpp
)

target_include_directories(packet-test-1 PRIVATE src)
target_include_directories(packet-test-1 PRIVATE kc1fsz-tools-cpp/include)
target_include_directories(packet-test-1 PRIVATE gsm-0610-codec/include)

# ------ common-test-1 -------------------------------------------------------

add_executable(common-test-1
  tests/common-test-1.cpp
  src/common.cpp
  kc1fsz-tools-cpp/src/win32/Win32PerfTimer.cpp
  kc1fsz-tools-cpp/src/Common.cpp
) 

target_include_directories(common-test-1 PRIVATE src)
target_include_directories(common-test-1 PRIVATE kc1fsz-tools-cpp/include)

# ------ audio-test-1 --------------------------------------------------------

add_executable(audio-test-1
  tests/audio-test-1.cpp
  src/common.cpp
)

target_include_directories(audio-test-1 PRIVATE src)
target_link_libraries(audio-test-1 winmm)

# ------ audio-test-2 --------------------------------------------------------

add_executable(audio-test-2
  tests/audio-test-2.cpp
  src/common.cpp
  kc1fsz-tools-cpp/src/win32/Win32PerfTimer.cpp
  src/contexts/W32AudioOutputContext.cpp
)

target_include_directories(audio-test-2 PRIVATE src)
target_include_directories(audio-test-2 PRIVATE kc1fsz-tools-cpp/include)
target_link_libraries(audio-test-2 winmm)

# ------ machine-test-1 -------------------------------------------------------

add_executable(machine-test-1
  tests/machine-test-1.cpp
  tests/TestContext.cpp
  src/common.cpp
  src/machines/RootMachine.cpp
  src/machines/LogonMachine.cpp
  src/machines/LookupMachine.cpp
  src/machines/QSOConnectMachine.cpp
  src/machines/QSOFlowMachine.cpp
  src/machines/WaitMachine.cpp
  kc1fsz-tools-cpp/src/Common.cpp
  gsm-0610-codec/src/fixed_math.cpp
  gsm-0610-codec/src/wav_util.cpp
  gsm-0610-codec/src/Parameters.cpp
  gsm-0610-codec/src/Encoder.cpp
  gsm-0610-codec/src/Decoder.cpp
)

target_include_directories(machine-test-1 PRIVATE src)
target_include_directories(machine-test-1 PRIVATE kc1fsz-tools-cpp/include)
target_include_directories(machine-test-1 PRIVATE gsm-0610-codec/include)

# ------ client-test-2 -------------------------------------------------------

add_executable(client-test-2
  tests/client-test-2.cpp
  tests/TestContext.cpp
  src/common.cpp
  src/contexts/SocketContext.cpp
  src/contexts/W32AudioOutputContext.cpp
  src/machines/RootMachine.cpp
  src/machines/LogonMachine.cpp
  src/machines/LookupMachine.cpp
  src/machines/QSOConnectMachine.cpp
  src/machines/QSOFlowMachine.cpp
  src/machines/WaitMachine.cpp
  kc1fsz-tools-cpp/src/Common.cpp
  kc1fsz-tools-cpp/src/win32/Win32PerfTimer.cpp
  gsm-0610-codec/src/fixed_math.cpp
  gsm-0610-codec/src/wav_util.cpp
  gsm-0610-codec/src/Parameters.cpp
  gsm-0610-codec/src/Encoder.cpp
  gsm-0610-codec/src/Decoder.cpp
)

target_include_directories(client-test-2 PRIVATE src)
target_include_directories(client-test-2 PRIVATE kc1fsz-tools-cpp/include)
target_include_directories(client-test-2 PRIVATE gsm-0610-codec/include)
target_link_libraries(client-test-2 winmm)

# ------ exception-test-1 ----------------------------------------------------

add_executable(exception-test-1
  tests/exception-test-1.cpp
)

if(DEFINED ENV{PICO_BUILD})
pico_enable_stdio_usb(exception-test-1 1)
pico_enable_stdio_uart(exception-test-1 0)
target_compile_definitions(exception-test-1 PUBLIC PICO_BUILD=1)
target_link_libraries(exception-test-1 pico_stdlib hardware_i2c hardware_adc pico_multicore)
endif()

# ------ esp32at-test-1 ------------------------------------------------------

add_executable(esp32at-test-1
tests/esp32at-test-1.cpp
src/ATResponseProcessor.cpp
  kc1fsz-tools-cpp/src/Common.cpp
  kc1fsz-tools-cpp/src/rp2040/PicoPollTimer.cpp
  kc1fsz-tools-cpp/src/rp2040/PicoPerfTimer.cpp
  kc1fsz-tools-cpp/src/rp2040/PicoUartChannel.cpp
)

if(DEFINED ENV{PICO_BUILD})
pico_enable_stdio_usb(esp32at-test-1 1)
# Watch out!  Don't let the STDIO get sent to the UART
pico_enable_stdio_uart(esp32at-test-1 0)
target_compile_definitions(esp32at-test-1 PUBLIC PICO_BUILD=1)
target_link_libraries(esp32at-test-1 pico_stdlib hardware_i2c hardware_adc pico_multicore)
endif()

target_include_directories(esp32at-test-1 PRIVATE src)
target_include_directories(esp32at-test-1 PRIVATE kc1fsz-tools-cpp/include)

# ------ esp32at-test-2 ------------------------------------------------------

add_executable(esp32at-test-2
  tests/esp32at-test-2.cpp
  src/ATResponseProcessor.cpp
  kc1fsz-tools-cpp/src/Common.cpp
  kc1fsz-tools-cpp/src/rp2040/PicoPollTimer.cpp
  kc1fsz-tools-cpp/src/rp2040/PicoPerfTimer.cpp
  kc1fsz-tools-cpp/src/rp2040/PicoUartChannel.cpp
)

if(DEFINED ENV{PICO_BUILD})
pico_enable_stdio_usb(esp32at-test-2 1)
# Watch out!  Don't let the STDIO get sent to the UART
pico_enable_stdio_uart(esp32at-test-2 0)
target_compile_definitions(esp32at-test-2 PRIVATE PARAM_ASSERTIONS_ENABLE_ALL=1)
target_compile_definitions(esp32at-test-2 PRIVATE PICO_BUILD=1)
target_link_libraries(esp32at-test-2 pico_stdlib hardware_i2c hardware_adc pico_multicore)
endif()

target_include_directories(esp32at-test-2 PRIVATE src)
target_include_directories(esp32at-test-2 PRIVATE kc1fsz-tools-cpp/include)

# ------ socket-test-2 ------------------------------------------------------
#
# Runs on the RP2040

add_executable(socket-test-2
  tests/socket-test-2.cpp
  src/ATResponseProcessor.cpp
  src/common.cpp
  src/contexts/ESP32CommContext.cpp
  kc1fsz-tools-cpp/src/Common.cpp
  kc1fsz-tools-cpp/src/rp2040/PicoPollTimer.cpp
  kc1fsz-tools-cpp/src/rp2040/PicoPerfTimer.cpp
  kc1fsz-tools-cpp/src/rp2040/PicoUartChannel.cpp
)

if(DEFINED ENV{PICO_BUILD})
pico_enable_stdio_usb(socket-test-2 1)
# Watch out!  Don't let the STDIO get sent to the UART
pico_enable_stdio_uart(socket-test-2 0)
target_compile_definitions(socket-test-2 PRIVATE PARAM_ASSERTIONS_ENABLE_ALL=1)
target_compile_definitions(socket-test-2 PRIVATE PICO_BUILD=1)
target_link_libraries(socket-test-2 pico_stdlib hardware_i2c hardware_adc pico_multicore)
endif()

target_include_directories(socket-test-2 PRIVATE src)
target_include_directories(socket-test-2 PRIVATE kc1fsz-tools-cpp/include)

# ------ client-test-2p ------------------------------------------------------
#
# Runs on the RP2040

add_executable(client-test-2p
  tests/client-test-2p.cpp
  src/ATResponseProcessor.cpp
  src/common.cpp
  src/contexts/ESP32CommContext.cpp
  src/machines/RootMachine.cpp
  src/machines/LogonMachine.cpp
  src/machines/LookupMachine.cpp
  src/machines/QSOConnectMachine.cpp
  src/machines/QSOFlowMachine.cpp
  src/machines/WaitMachine.cpp
  kc1fsz-tools-cpp/src/Common.cpp
  kc1fsz-tools-cpp/src/rp2040/PicoPollTimer.cpp
  kc1fsz-tools-cpp/src/rp2040/PicoPerfTimer.cpp
  kc1fsz-tools-cpp/src/rp2040/PicoUartChannel.cpp
  gsm-0610-codec/src/fixed_math.cpp
  gsm-0610-codec/src/wav_util.cpp
  gsm-0610-codec/src/Parameters.cpp
  gsm-0610-codec/src/Encoder.cpp
  gsm-0610-codec/src/Decoder.cpp
)

if(DEFINED ENV{PICO_BUILD})
pico_enable_stdio_usb(client-test-2p 1)
# Watch out!  Don't let the STDIO get sent to the UART
pico_enable_stdio_uart(client-test-2p 0)
target_compile_definitions(client-test-2p PRIVATE PARAM_ASSERTIONS_ENABLE_ALL=1)
target_compile_definitions(client-test-2p PRIVATE PICO_BUILD=1)
target_link_libraries(client-test-2p pico_stdlib hardware_i2c hardware_adc pico_multicore)
endif()

target_include_directories(client-test-2p PRIVATE src)
target_include_directories(client-test-2p PRIVATE kc1fsz-tools-cpp/include)
target_include_directories(client-test-2p PRIVATE gsm-0610-codec/include)
