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

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
	string(REPLACE "." ";" _project_ver_parts "${PROJECT_VER}")
	list(LENGTH _project_ver_parts _project_ver_count)
	if(_project_ver_count GREATER 0)
		list(GET _project_ver_parts 0 _ver_major)
	else()
		set(_ver_major 1)
	endif()
	if(_project_ver_count GREATER 1)
		list(GET _project_ver_parts 1 _ver_minor)
	else()
		set(_ver_minor 0)
	endif()
	if(_project_ver_count GREATER 2)
		list(GET _project_ver_parts 2 _ver_patch)
	else()
		set(_ver_patch 0)
	endif()

	math(EXPR OTA_FILE_VERSION_NUM "(${_ver_major} << 24) | (${_ver_minor} << 16) | (${_ver_patch} << 8)")

	add_custom_target(zigbee_ota_image
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/image_builder_tool.py
			-c ${CMAKE_SOURCE_DIR}/releases/${CMAKE_PROJECT_NAME}_${PROJECT_VER}.ota
			-v ${OTA_FILE_VERSION_NUM}
			-m 0x1234
			-i 0x1011
			-f ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
			--header_string "Kommando ${PROJECT_VER}"
			-s 0x0002
			-t 0x0000
		DEPENDS app
		COMMENT "Generating Zigbee OTA image in releases/"
		VERBATIM
	)
endif()