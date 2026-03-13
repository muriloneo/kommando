# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
# Force the project to use the custom partition table in the repo's partition_table folder.
# This ensures the generated partition-table.bin picks up our `zb_storage` entries.
set(PARTITION_TABLE_CUSTOM "${CMAKE_SOURCE_DIR}/partition_table/partition-table.csv")
# Firmware semantic version (used by ESP-IDF PROJECT_VER metadata)
set(PROJECT_VER "1.0.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(kommando)