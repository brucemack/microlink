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
project(radlib C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
add_compile_options(-fstack-protector-all -Wall -Wpedantic -g)

file(MAKE_DIRECTORY tmp)

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

add_executable(common-test-1
  tests/common-test-1.cpp
  src/common.cpp
)

target_include_directories(common-test-1 PRIVATE src)

# ------ client-test-1 -------------------------------------------------------

add_executable(client-test-1
  tests/client-test-1.cpp
  src/common.cpp
  gsm-0610-codec/src/fixed_math.cpp
  gsm-0610-codec/src/wav_util.cpp
  gsm-0610-codec/src/Parameters.cpp
  gsm-0610-codec/src/Encoder.cpp
  gsm-0610-codec/src/Decoder.cpp
)

target_include_directories(client-test-1 PRIVATE src)
target_include_directories(client-test-1 PRIVATE gsm-0610-codec/include)
target_include_directories(client-test-1 PRIVATE gsm-0610-codec/src)






