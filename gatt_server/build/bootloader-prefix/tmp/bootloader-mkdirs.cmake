# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/Users/ensayne/esp/v5.2.2/esp-idf/components/bootloader/subproject"
  "/Users/ensayne/AndroidStudioProjects/sayne_hfp_project/gatt_server/build/bootloader"
  "/Users/ensayne/AndroidStudioProjects/sayne_hfp_project/gatt_server/build/bootloader-prefix"
  "/Users/ensayne/AndroidStudioProjects/sayne_hfp_project/gatt_server/build/bootloader-prefix/tmp"
  "/Users/ensayne/AndroidStudioProjects/sayne_hfp_project/gatt_server/build/bootloader-prefix/src/bootloader-stamp"
  "/Users/ensayne/AndroidStudioProjects/sayne_hfp_project/gatt_server/build/bootloader-prefix/src"
  "/Users/ensayne/AndroidStudioProjects/sayne_hfp_project/gatt_server/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/ensayne/AndroidStudioProjects/sayne_hfp_project/gatt_server/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/ensayne/AndroidStudioProjects/sayne_hfp_project/gatt_server/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
