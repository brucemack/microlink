# MicroLink EchoLink Station
# Copyright (C) 2024, Bruce MacKinnon 
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

add_executable(el-test-1
  tests/unit-test-1.cpp
  src/fixed_math.cpp
  src/wav_util.cpp
  src/Parameters.cpp
  src/Encoder.cpp
  src/Decoder.cpp
)

target_include_directories(gsm-test-1 PUBLIC include)
target_include_directories(gsm-test-1 PRIVATE src)




