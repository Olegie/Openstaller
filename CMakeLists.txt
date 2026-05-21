cmake_minimum_required(VERSION 3.20)

project(Openstaller
    VERSION 0.1.0
    DESCRIPTION "C and Assembly installer/uninstaller generator"
    LANGUAGES C)

option(OPENSTALLER_BUILD_GUI "Build the native Windows GUI" ON)
option(OPENSTALLER_FORCE_C_HASH "Disable the Assembly hash kernel" OFF)
option(OPENSTALLER_USE_ZLIB "Use zlib/deflate compression for embedded payload archives when available" ON)
option(OPENSTALLER_WIN2000_COMPAT "Build Windows binaries with a Windows 2000 API/subsystem compatibility floor" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(CTest)

if(WIN32)
    enable_language(RC)
endif()

if(OPENSTALLER_WIN2000_COMPAT AND NOT WIN32)
    message(WARNING "OPENSTALLER_WIN2000_COMPAT only affects Windows builds.")
endif()

set(OPENSTALLER_WIN32_COMPAT_DEFINITIONS)
if(WIN32 AND OPENSTALLER_WIN2000_COMPAT)
    set(OPENSTALLER_WIN32_COMPAT_DEFINITIONS
        WINVER=0x0500
        _WIN32_WINNT=0x0500
        _WIN32_IE=0x0500
        NTDDI_VERSION=0x05000000
        OPENSTALLER_WIN2000_COMPAT=1)
    if(MSVC)
        message(WARNING "OPENSTALLER_WIN2000_COMPAT sets Win32 API/subsystem compatibility, but modern MSVC runtimes do not target Windows 2000. Use a suitable legacy-capable toolchain for actual Windows 2000 binaries.")
    endif()
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(WARNING "Windows 2000 runtime compatibility is a 32-bit Win32 target. The current toolchain is not 32-bit, so subsystem version 5.00 will not be forced.")
    endif()
endif()

function(openstaller_apply_windows_floor target subsystem)
    if(WIN32 AND OPENSTALLER_WIN2000_COMPAT)
        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            if(MSVC)
                target_link_options(${target} PRIVATE "/SUBSYSTEM:${subsystem},5.00")
            elseif(MINGW)
                target_link_options(${target} PRIVATE "-Wl,--major-subsystem-version,5,--minor-subsystem-version,0")
            endif()
        endif()
    endif()
endfunction()

add_library(openstaller_core
    src/core/openstaller.c
    src/core/openstaller_config.c
    src/core/openstaller_install.c
    src/core/openstaller_install_fs.c
    src/core/openstaller_install_registration.c
    src/core/openstaller_icon.c
    src/core/openstaller_icon_resource.c
    src/core/openstaller_manifest.c
    src/core/openstaller_online.c
    src/core/openstaller_pack.c
    src/core/openstaller_pack_compress.c
    src/core/openstaller_platform.c
    src/core/openstaller_rollback.c
    src/core/openstaller_scripts.c
    src/core/openstaller_scripts_unix.c
    src/core/openstaller_scripts_windows.c
    src/core/openstaller_version.c
    src/asm/openstaller_hash_fallback.c)

target_include_directories(openstaller_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(openstaller_core
    PUBLIC
        ${OPENSTALLER_WIN32_COMPAT_DEFINITIONS})

if(MSVC)
    target_compile_options(openstaller_core PRIVATE $<$<COMPILE_LANGUAGE:C>:/W4>)
    target_compile_definitions(openstaller_core PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(openstaller_core PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wall -Wextra -Wpedantic>)
endif()

if(OPENSTALLER_USE_ZLIB)
    find_package(ZLIB QUIET)
    if(NOT ZLIB_FOUND)
        get_filename_component(OPENSTALLER_COMPILER_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
        get_filename_component(OPENSTALLER_COMPILER_PREFIX "${OPENSTALLER_COMPILER_DIR}" DIRECTORY)
        set(OPENSTALLER_ZLIB_HINTS
            "$ENV{MINGW_PREFIX}"
            "${OPENSTALLER_COMPILER_PREFIX}"
            "${OPENSTALLER_COMPILER_PREFIX}/x86_64-w64-mingw32"
            "${OPENSTALLER_COMPILER_PREFIX}/mingw64")
        find_path(OPENSTALLER_ZLIB_INCLUDE_DIR zlib.h
            HINTS ${OPENSTALLER_ZLIB_HINTS}
            PATH_SUFFIXES include)
        find_library(OPENSTALLER_ZLIB_LIBRARY
            NAMES z zlib zlibstatic
            HINTS ${OPENSTALLER_ZLIB_HINTS}
            PATH_SUFFIXES lib)
        if(OPENSTALLER_ZLIB_INCLUDE_DIR AND OPENSTALLER_ZLIB_LIBRARY)
            add_library(OpenstallerZlib UNKNOWN IMPORTED)
            set_target_properties(OpenstallerZlib PROPERTIES
                IMPORTED_LOCATION "${OPENSTALLER_ZLIB_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${OPENSTALLER_ZLIB_INCLUDE_DIR}")
            set(ZLIB_FOUND TRUE)
            set(OPENSTALLER_ZLIB_TARGET OpenstallerZlib)
        endif()
    else()
        set(OPENSTALLER_ZLIB_TARGET ZLIB::ZLIB)
    endif()
    if(ZLIB_FOUND)
        target_link_libraries(openstaller_core PRIVATE ${OPENSTALLER_ZLIB_TARGET})
        target_compile_definitions(openstaller_core PUBLIC OPENSTALLER_HAVE_ZLIB=1)
    endif()
endif()

if(WIN32)
    target_link_libraries(openstaller_core PUBLIC wininet ole32 uuid)
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" OPENSTALLER_ARCH)
if(NOT OPENSTALLER_FORCE_C_HASH AND OPENSTALLER_ARCH MATCHES "^(x86_64|amd64|x64)$")
    if(MSVC)
        enable_language(ASM_MASM)
        target_sources(openstaller_core PRIVATE src/asm/openstaller_hash_x86_64_msvc.asm)
    else()
        enable_language(ASM)
        target_sources(openstaller_core PRIVATE src/asm/openstaller_hash_x86_64.S)
    endif()
    target_compile_definitions(openstaller_core PRIVATE OPENSTALLER_USE_ASM=1)
endif()

add_executable(openstaller-cli
    src/cli/main.c)
target_link_libraries(openstaller-cli PRIVATE openstaller_core)
openstaller_apply_windows_floor(openstaller-cli CONSOLE)
if(WIN32)
    target_link_libraries(openstaller-cli PRIVATE advapi32)
endif()

if(WIN32 AND OPENSTALLER_BUILD_GUI)
    add_executable(openstaller-runtime WIN32
        src/runtime/win32_runtime_main.c
        src/runtime/win32_runtime_controls.c
        src/runtime/win32_runtime_elevation.c
        src/runtime/win32_runtime_images.c
        src/runtime/win32_runtime_legacy.c
        src/runtime/win32_runtime_modern.c
        src/runtime/win32_runtime_pages.c
        src/runtime/win32_runtime_util.c
        src/win32/win32_i18n.c
        src/win32/openstaller_runtime.rc)
    target_include_directories(openstaller-runtime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/win32)
    target_link_libraries(openstaller-runtime PRIVATE openstaller_core comctl32 comdlg32 shell32 ole32 advapi32)
    openstaller_apply_windows_floor(openstaller-runtime WINDOWS)

    add_executable(openstaller WIN32
        src/gui/win32_builder_main.c
        src/gui/win32_builder_controls.c
        src/gui/win32_builder_pages.c
        src/gui/win32_builder_preview.c
        src/win32/win32_i18n.c
        src/win32/openstaller_builder.rc)
    target_include_directories(openstaller PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
    target_link_libraries(openstaller PRIVATE openstaller_core comctl32 comdlg32 shell32 ole32 advapi32)
    openstaller_apply_windows_floor(openstaller WINDOWS)
endif()

if(BUILD_TESTING)
    add_executable(test_openstaller_core
        tests/test_core.c)
    target_link_libraries(test_openstaller_core PRIVATE openstaller_core)
    add_test(NAME openstaller_core COMMAND test_openstaller_core)

    add_executable(test_unix_scripts
        tests/test_unix_scripts.c)
    target_link_libraries(test_unix_scripts PRIVATE openstaller_core)
    add_test(NAME unix_scripts COMMAND test_unix_scripts)

    add_executable(test_rollback
        tests/test_rollback.c)
    target_include_directories(test_rollback PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
    target_link_libraries(test_rollback PRIVATE openstaller_core)
    add_test(NAME rollback COMMAND test_rollback)

    if(WIN32 AND OPENSTALLER_BUILD_GUI)
        add_executable(test_embedded_package
            tests/test_embedded_package.c)
        target_link_libraries(test_embedded_package PRIVATE openstaller_core advapi32 version)
        add_test(NAME embedded_package
            COMMAND test_embedded_package
                $<TARGET_FILE:openstaller-runtime>
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/sample_payload)
    endif()
endif()
