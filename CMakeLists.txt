cmake_minimum_required(VERSION 3.20)
project(CR3WICCodec VERSION 1.3.1 LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "CR3WICCodec is a Windows Imaging Component (WIC) COM DLL and only supports Windows.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "CR3WICCodec requires a 64-bit (x64 or ARM64) build configuration.")
endif()

string(TOUPPER "${CMAKE_SYSTEM_PROCESSOR}" _CR3_PROC)
if(_CR3_PROC MATCHES "ARM64|AARCH64")
    set(_CR3_OPENMP_ARCH "arm64")
else()
    set(_CR3_OPENMP_ARCH "x64")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CR3_STATIC_CRT_TOOLS "Link test_decoder statically against MSVC CRT" ON)

set(LIBRAW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/LibRaw" CACHE PATH "Path to LibRaw source/build directory")
set(LIBRAW_INCLUDE_DIR "${LIBRAW_ROOT}")
set(LIBRAW_IMPORT_LIB "${LIBRAW_ROOT}/lib/libraw.lib")
set(LIBRAW_RUNTIME_DLL "${LIBRAW_ROOT}/bin/libraw.dll")

if(NOT EXISTS "${LIBRAW_IMPORT_LIB}" OR NOT EXISTS "${LIBRAW_RUNTIME_DLL}")
    message(FATAL_ERROR
        "Prebuilt LibRaw x64 binaries not found at:\n"
        "  ${LIBRAW_IMPORT_LIB}\n"
        "  ${LIBRAW_RUNTIME_DLL}\n"
        "Run scripts\\build.ps1 or build LibRaw from an x64 VS Developer Prompt via:\n"
        "  cd third_party\\LibRaw && nmake -f Makefile.msvc \"COPT_OPT=/openmp\" bin\\libraw.dll lib\\libraw.lib")
endif()

add_library(LibRaw::LibRaw SHARED IMPORTED GLOBAL)
set_target_properties(LibRaw::LibRaw PROPERTIES
    IMPORTED_IMPLIB "${LIBRAW_IMPORT_LIB}"
    IMPORTED_LOCATION "${LIBRAW_RUNTIME_DLL}"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBRAW_INCLUDE_DIR}"
)

# Locate vcomp140.dll when LibRaw is built with OpenMP
set(VCOMP140_DLL "" CACHE FILEPATH "Optional path to ${_CR3_OPENMP_ARCH} vcomp140.dll (OpenMP runtime)")
if(NOT VCOMP140_DLL AND DEFINED ENV{VCToolsRedistDir})
    file(GLOB _vcomp_candidates "$ENV{VCToolsRedistDir}/${_CR3_OPENMP_ARCH}/Microsoft.VC*.OpenMP/vcomp140.dll")
    if(_vcomp_candidates)
        list(GET _vcomp_candidates 0 VCOMP140_DLL)
    endif()
endif()
if(NOT VCOMP140_DLL)
    file(GLOB _vcomp_candidates
        "C:/Program Files/Microsoft Visual Studio/*/Community/VC/Redist/MSVC/*/${_CR3_OPENMP_ARCH}/Microsoft.VC*.OpenMP/vcomp140.dll"
        "C:/Program Files/Microsoft Visual Studio/*/Enterprise/VC/Redist/MSVC/*/${_CR3_OPENMP_ARCH}/Microsoft.VC*.OpenMP/vcomp140.dll"
        "C:/Program Files/Microsoft Visual Studio/*/BuildTools/VC/Redist/MSVC/*/${_CR3_OPENMP_ARCH}/Microsoft.VC*.OpenMP/vcomp140.dll"
        "C:/Program Files (x86)/Microsoft Visual Studio/*/BuildTools/VC/Redist/MSVC/*/${_CR3_OPENMP_ARCH}/Microsoft.VC*.OpenMP/vcomp140.dll"
    )
    if(_vcomp_candidates)
        list(GET _vcomp_candidates 0 VCOMP140_DLL)
    endif()
endif()

# ----------------------------------------------------------------------------
# CR3WICCodec.dll - Native WIC COM Decoder DLL
# ----------------------------------------------------------------------------
add_library(CR3WICCodec SHARED
    src/DllMain.cpp
    src/ClassFactory.cpp
    src/ClassFactory.h
    src/WicIStreamDatastream.cpp
    src/WicIStreamDatastream.h
    src/LibRawProcessor.cpp
    src/LibRawProcessor.h
    src/CR3Decoder.cpp
    src/CR3Decoder.h
    src/Registry.cpp
    src/Registry.h
    src/Guid.h
    src/CR3Codec.def
    src/CR3Codec.rc
)

set_target_properties(CR3WICCodec PROPERTIES
    OUTPUT_NAME "CR3WICCodec"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

target_compile_definitions(CR3WICCodec PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    CR3_VER_MAJOR=${PROJECT_VERSION_MAJOR}
    CR3_VER_MINOR=${PROJECT_VERSION_MINOR}
    CR3_VER_PATCH=${PROJECT_VERSION_PATCH}
)

target_compile_options(CR3WICCodec PRIVATE
    /W4
    /EHsc
    /utf-8
)

target_link_libraries(CR3WICCodec PRIVATE
    LibRaw::LibRaw
    delayimp
    windowscodecs
    ole32
    advapi32
    shell32
    shlwapi
)

target_link_options(CR3WICCodec PRIVATE
    "/DELAYLOAD:libraw.dll"
)

add_custom_command(TARGET CR3WICCodec POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LIBRAW_RUNTIME_DLL}"
        "$<TARGET_FILE_DIR:CR3WICCodec>/libraw.dll"
    COMMENT "Copying libraw.dll alongside CR3WICCodec.dll"
)

if(VCOMP140_DLL AND EXISTS "${VCOMP140_DLL}")
    message(STATUS "Found OpenMP runtime: ${VCOMP140_DLL}")
    add_custom_command(TARGET CR3WICCodec POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCOMP140_DLL}"
            "$<TARGET_FILE_DIR:CR3WICCodec>/vcomp140.dll"
        COMMENT "Copying vcomp140.dll alongside CR3WICCodec.dll"
    )
endif()

# ----------------------------------------------------------------------------
# test_decoder.exe - Pure Windows WIC verification CLI tool
# ----------------------------------------------------------------------------
add_executable(test_decoder
    tools/test_decoder.cpp
    src/Guid.h
)

set_target_properties(test_decoder PROPERTIES
    OUTPUT_NAME "test_decoder"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(CR3_STATIC_CRT_TOOLS)
    set_property(TARGET test_decoder PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_compile_definitions(test_decoder PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_compile_options(test_decoder PRIVATE
    /W4
    /EHsc
    /utf-8
)

target_link_libraries(test_decoder PRIVATE
    windowscodecs
    ole32
    shlwapi
)

# ----------------------------------------------------------------------------
# Install.exe & Uninstall.exe - Native UAC-elevated one-click GUI installers
# ----------------------------------------------------------------------------
foreach(_app IN ITEMS Install Uninstall)
    if(_app STREQUAL "Install")
        set(_src tools/install_app.cpp)
    else()
        set(_src tools/uninstall_app.cpp)
    endif()

    add_executable(${_app} WIN32
        ${_src}
        tools/SetupCommon.h
        src/Guid.h
    )
    set_target_properties(${_app} PROPERTIES
        OUTPUT_NAME "${_app}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    target_compile_definitions(${_app} PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    target_compile_options(${_app} PRIVATE
        /W4
        /EHsc
        /utf-8
    )
    target_link_libraries(${_app} PRIVATE
        ole32
        shell32
        advapi32
    )
    target_link_options(${_app} PRIVATE
        "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'"
    )
endforeach()
