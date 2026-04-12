# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Espressif/frameworks/esp-idf-v5.3.1/components/bootloader/subproject"
  "C:/IDF/green_charging/green-charging/build/bootloader"
  "C:/IDF/green_charging/green-charging/build/bootloader-prefix"
  "C:/IDF/green_charging/green-charging/build/bootloader-prefix/tmp"
  "C:/IDF/green_charging/green-charging/build/bootloader-prefix/src/bootloader-stamp"
  "C:/IDF/green_charging/green-charging/build/bootloader-prefix/src"
  "C:/IDF/green_charging/green-charging/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/IDF/green_charging/green-charging/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/IDF/green_charging/green-charging/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
