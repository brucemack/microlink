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

cmake_minimum_required(VERSION 3.13)
if (TARGET_GROUP STREQUAL pico)
include(pico_sdk_import.cmake)
endif()
project(microlink C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
file(MAKE_DIRECTORY tmp)

if (TARGET_GROUP STREQUAL pico)
pico_sdk_init()
endif()

if (TARGET_GROUP STREQUAL pico)
message("----- PICO build ---------------------------------------------------------------")
add_compile_options(-fstack-protector-all -Wall -Wpedantic -g)
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
)

target_include_directories(socket-test-1 PRIVATE src)

# ------ packet-test-1 -------------------------------------------------------

add_executable(packet-test-1
  tests/packet-test-1.cpp
  src/common.cpp
  gsm-0610-codec/src/fixed_math.cpp
  gsm-0610-codec/src/wav_util.cpp
  gsm-0610-codec/src/Parameters.cpp
  gsm-0610-codec/src/Encoder.cpp
  gsm-0610-codec/src/Decoder.cpp
)

target_include_directories(packet-test-1 PRIVATE src)
target_include_directories(packet-test-1 PRIVATE gsm-0610-codec/include)
target_include_directories(packet-test-1 PRIVATE gsm-0610-codec/src)

# ------ common-test-1 -------------------------------------------------------

add_executable(common-test-1
  tests/common-test-1.cpp
  src/common.cpp
)

target_include_directories(common-test-1 PRIVATE src)

# ------ machine-test-1 -------------------------------------------------------

add_executable(machine-test-1
  tests/machine-test-1.cpp
  tests/TestContext.cpp
  src/common.cpp
  src/Context.cpp
  src/machines/RootMachine.cpp
  src/machines/LogonMachine.cpp
  src/machines/LookupMachine.cpp
  src/machines/QSOConnectMachine.cpp
  src/machines/QSOFlowMachine.cpp
  src/machines/WaitMachine.cpp
)

target_include_directories(machine-test-1 PRIVATE src)
target_include_directories(machine-test-1 PRIVATE gsm-0610-codec/include)
target_include_directories(machine-test-1 PRIVATE gsm-0610-codec/src)

# ------ client-test-1 -------------------------------------------------------

#add_executable(client-test-1
#  tests/client-test-1.cpp
#  src/common.cpp
#  gsm-0610-codec/src/fixed_math.cpp
#  gsm-0610-codec/src/wav_util.cpp
#  gsm-0610-codec/src/Parameters.cpp
#  gsm-0610-codec/src/Encoder.cpp
#  gsm-0610-codec/src/Decoder.cpp
#)

#target_include_directories(client-test-1 PRIVATE src)
#target_include_directories(client-test-1 PRIVATE gsm-0610-codec/include)
#target_include_directories(client-test-1 PRIVATE gsm-0610-codec/src)

# ------ exception-test-1 ----------------------------------------------------

add_executable(exception-test-1
  tests/exception-test-1.cpp
)

if (TARGET_GROUP STREQUAL pico)
pico_enable_stdio_usb(exception-test-1 1)
pico_enable_stdio_uart(exception-test-1 1)
target_compile_definitions(exception-test-1 PUBLIC PICO_BUILD=1)
target_link_libraries(exception-test-1 pico_stdlib hardware_i2c hardware_adc pico_multicore)
endif()


