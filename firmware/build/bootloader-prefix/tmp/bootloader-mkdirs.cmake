# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/bootloader/subproject")
  file(MAKE_DIRECTORY "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/bootloader/subproject")
endif()
file(MAKE_DIRECTORY
  "C:/codes/esp32/kommando/firmware/build/bootloader"
  "C:/codes/esp32/kommando/firmware/build/bootloader-prefix"
  "C:/codes/esp32/kommando/firmware/build/bootloader-prefix/tmp"
  "C:/codes/esp32/kommando/firmware/build/bootloader-prefix/src/bootloader-stamp"
  "C:/codes/esp32/kommando/firmware/build/bootloader-prefix/src"
  "C:/codes/esp32/kommando/firmware/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/codes/esp32/kommando/firmware/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/codes/esp32/kommando/firmware/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
