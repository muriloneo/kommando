# Install script for directory: C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/kommando")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "C:/Espressif/tools/riscv32-esp-elf/esp-15.2.0_20251204/riscv32-esp-elf/bin/riscv32-esp-elf-objdump.exe")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/psa" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_adjust_auto_enabled.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_adjust_config_dependencies.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_adjust_config_derived.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_adjust_config_key_pair_types.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_adjust_config_synonyms.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_builtin_composites.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_builtin_key_derivation.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_builtin_primitives.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_compat.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_config.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_driver_common.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_driver_contexts_composites.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_driver_contexts_key_derivation.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_driver_contexts_primitives.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_driver_random.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_extra.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_platform.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_sizes.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_struct.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_types.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_values.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/tf-psa-crypto" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/tf-psa-crypto/build_info.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/tf-psa-crypto/version.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mbedtls" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/../drivers/builtin/include/mbedtls/config_adjust_legacy_crypto.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/../drivers/builtin/include/mbedtls/private_access.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/mbedtls/asn1.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/mbedtls/asn1write.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/mbedtls/base64.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/mbedtls/compat-3-crypto.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/mbedtls/constant_time.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/mbedtls/lms.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/mbedtls/md.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/mbedtls/memory_buffer_alloc.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/mbedtls/nist_kw.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/mbedtls/pem.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/mbedtls/pk.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/mbedtls/platform.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/mbedtls/platform_time.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/mbedtls/platform_util.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/mbedtls/psa_util.h"
    "C:/Espressif/.espressif/v6.0-beta2/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/include/mbedtls/threading.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/codes/esp32/kommando/firmware/build/esp-idf/mbedtls/mbedtls/tf-psa-crypto/include/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
