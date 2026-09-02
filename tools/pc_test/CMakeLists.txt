# PC Test Tool for DLMS/COSEM Meter Communication
# Builds on Windows using standard CMake (NOT ESP-IDF)
# Reuses the portable DLMS protocol code from components/dlms_client/
cmake_minimum_required(VERSION 3.16)
project(dlms_pc_test C)

set(CMAKE_C_STANDARD 11)

# ── Source files ──
# Reuse portable protocol code directly from ESP-IDF components
set(DLMS_COMPONENT_DIR "${CMAKE_SOURCE_DIR}/../../components/dlms_client")

add_executable(dlms_test
    main.c
    serial_port.c
    dlms_pc_client.c
    # Reuse these directly - they are pure C with only esp_log.h dependency
    ${DLMS_COMPONENT_DIR}/dlms_hdlc.c
    ${DLMS_COMPONENT_DIR}/dlms_axdr.c
    ${DLMS_COMPONENT_DIR}/dlms_helpers.c
)

# ── Include paths ──
# IMPORTANT: compat/ must come FIRST so our esp_log.h shim overrides the real one
target_include_directories(dlms_test PRIVATE
    ${CMAKE_SOURCE_DIR}/compat       # ESP-IDF API shims (esp_log.h, etc.)
    ${CMAKE_SOURCE_DIR}              # Local headers (serial_port.h, etc.)
    ${DLMS_COMPONENT_DIR}/include    # dlms_types.h, dlms_hdlc.h, etc.
)

# ── Windows-specific ──
if(WIN32)
    target_compile_definitions(dlms_test PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Enable all warnings
if(MSVC)
    target_compile_options(dlms_test PRIVATE /W3)
else()
    target_compile_options(dlms_test PRIVATE -Wall -Wextra)
    if(MINGW OR WIN32)
        target_link_options(dlms_test PRIVATE -mconsole)
    endif()
endif()
