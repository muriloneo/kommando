cmake_minimum_required(VERSION 3.16)
project(kommando_sim C)
set(CMAKE_C_STANDARD 11)

# ============================================================
# SDL2
# ============================================================
find_package(SDL2 REQUIRED)

# ============================================================
# LVGL source tree
# ============================================================
set(LVGL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/lvgl__lvgl")

# Collect all LVGL C files, then strip platform-specific drivers we don't use
file(GLOB_RECURSE LVGL_SOURCES "${LVGL_DIR}/src/*.c")
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/(evdev|libinput|nuttx|opengles|qnx|uefi|wayland|x11|windows)/.*")

# ============================================================
# Simulator executable
# ============================================================
add_executable(kommando_sim
    main.c
    ../main/ui/ui_panel.c
    ../main/ui/ui_state.c
    ../main/ui/ui_theme.c
    ${LVGL_SOURCES}
)

# Tell LVGL to look for lv_conf.h relative to the source root (this directory)
target_compile_definitions(kommando_sim PRIVATE
    LV_CONF_INCLUDE_SIMPLE       # LVGL will do: #include "lv_conf.h"
)

target_include_directories(kommando_sim PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"   # lv_conf.h lives here
    "${LVGL_DIR}"                   # lvgl.h
    "${LVGL_DIR}/src"               # driver sub-headers
    "../main/ui"                    # ui_panel.h, ui_state.h, ui_config.h, ui_events.h
)

target_link_libraries(kommando_sim PRIVATE SDL2::SDL2)

# Silence LVGL's internal deprecation noise on GCC/Clang
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(kommando_sim PRIVATE -Wno-deprecated-declarations)
endif()

# ============================================================
# Build instructions (printed when CMake configures)
# ============================================================
message(STATUS "")
message(STATUS "Kommando UI Simulator targets:")
message(STATUS "  cmake --build . --target kommando_sim")
message(STATUS "  ./kommando_sim")
message(STATUS "")
message(STATUS "On Windows with vcpkg SDL2:")
message(STATUS "  cmake -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake ..")
message(STATUS "On Linux/macOS (SDL2 from package manager):")
message(STATUS "  cmake ..")
message(STATUS "")
