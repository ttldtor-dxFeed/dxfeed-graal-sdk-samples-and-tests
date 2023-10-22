# Copyright (c) 2023 Devexperts LLC.
# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.21)

if (POLICY CMP0092)
    cmake_policy(SET CMP0092 NEW)
endif ()

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

project(attach-detach-while-destruction)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CXX_EXTENSIONS OFF)
set(C_EXTENSIONS OFF)

if (NOT DEFINED DXFCXX_IS_ROOT_PROJECT)
    if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        set(DXFCXX_IS_ROOT_PROJECT ON)
        message(STATUS "CMake version: ${CMAKE_VERSION}")
    else ()
        set(DXFCXX_ROOT_PROJECT OFF)
    endif ()
endif ()

option(DXFCXX_USE_DXFEED_GRAAL_NATIVE_SDK_JFROG "" ON)
set(DXFEED_GRAAL_NATIVE_SDK_VERSION "1.0.6" CACHE STRING "")
set(DXFEED_GRAAL_NATIVE_SDK_JFROG_BASE_URL "https://dxfeed.jfrog.io/artifactory/maven-open/com/dxfeed/graal-native-sdk/" CACHE STRING "")

option(DXFCXX_ENABLE_ASAN_UBSAN "Enable address, UB sanitizers etc" OFF)
option(DXFCXX_ENABLE_VS_ASAN "Enable VS address sanitizer" OFF)
option(DXFCXX_USE_PRECOMPILED_HEADERS "Use precompiled headers" ON)

include(FetchContent)

set(DXFCXX_GRAAL_TARGET_PLATFORM "unknown-unknown" CACHE STRING "")
set(DXFCXX_TARGET_PLATFORM "unknown-unknown" CACHE STRING "")
set(DXFCXX_TARGET_OS "unknown" CACHE STRING "")
set(DXFCXX_GRAAL_TARGET_OS "unknown" CACHE STRING "")
set(DXFCXX_TARGET_CPU "unknown" CACHE STRING "")
set(DXFCXX_GRAAL_TARGET_CPU "unknown" CACHE STRING "")

include(ParseAndDetectPlatforms.cmake)

if (DEFINED ENV{DXFEED_GRAAL_NATIVE_SDK_URL})
    FetchContent_Declare(DxFeedGraalNativeSdk URL $ENV{DXFEED_GRAAL_NATIVE_SDK_URL})
else ()
    if (DXFCXX_USE_DXFEED_GRAAL_NATIVE_SDK_JFROG)
        set(DXFEED_GRAAL_NATIVE_SDK_URL "${DXFEED_GRAAL_NATIVE_SDK_JFROG_BASE_URL}${DXFEED_GRAAL_NATIVE_SDK_VERSION}/graal-native-sdk-${DXFEED_GRAAL_NATIVE_SDK_VERSION}")
    endif ()

    if (DXFCXX_GRAAL_TARGET_PLATFORM STREQUAL "unknown-unknown")
        message(ERROR "Unsupported platform!")
    else ()
        set(DXFEED_GRAAL_NATIVE_SDK_URL ${DXFEED_GRAAL_NATIVE_SDK_URL}-${DXFCXX_GRAAL_TARGET_PLATFORM}.zip)
    endif ()

    FetchContent_Declare(DxFeedGraalNativeSdk URL ${DXFEED_GRAAL_NATIVE_SDK_URL})
endif ()

FetchContent_MakeAvailable(DxFeedGraalNativeSdk)
# DxFeedGraalNativeSdk_SOURCE_DIR

#FetchContent_Declare(Process GIT_REPOSITORY "https://github.com/ttldtor/Process.git" GIT_TAG default)
#FetchContent_MakeAvailable(Process)

#FetchContent_Declare(Console GIT_REPOSITORY "https://github.com/ttldtor/Console.git" GIT_TAG default)
#FetchContent_MakeAvailable(Console)


if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_MACOSX_RPATH ON)
    set(CMAKE_SKIP_BUILD_RPATH ON)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH OFF)
    set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
    set(CMAKE_INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path;@executable_path;@executable_path/../Frameworks")
elseif (UNIX)
    set(CMAKE_SKIP_BUILD_RPATH ON)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH OFF)
    set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:$ORIGIN/../lib64:$ORIGIN/../lib:$ORIGIN")
endif ()

add_executable(${PROJECT_NAME} 
        src/main.cpp
)

add_library(dxfcxx::graal ALIAS DxFeedGraalNativeSdk)

target_include_directories(${PROJECT_NAME} PUBLIC include)

target_compile_options(${PROJECT_NAME}
        PUBLIC
        $<$<COMPILE_LANG_AND_ID:C,CXX,MSVC>:
        /W2
        >
)

target_compile_options(${PROJECT_NAME}
        PUBLIC
        $<$<COMPILE_LANG_AND_ID:C,CXX,GNU,Clang,AppleClang,Intel>:
        -fPIC
        -Wall
        -Wextra
        -pedantic
        >
)

target_link_options(${PROJECT_NAME}
        PUBLIC
        $<$<COMPILE_LANG_AND_ID:C,CXX,GNU,Clang,AppleClang,Intel>:
        -fPIC
        >)

target_link_libraries(${PROJECT_NAME} PUBLIC DxFeedGraalNativeSdk)

if (DXFCXX_ENABLE_ASAN_UBSAN)
    target_compile_options(${PROJECT_NAME} PRIVATE "-fsanitize=address" "-fsanitize=undefined")
    target_link_options(${PROJECT_NAME} PRIVATE "-fsanitize=address" "-fsanitize=undefined")
    target_link_libraries(${PROJECT_NAME} PUBLIC asan ubsan)
endif ()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:DxFeedGraalNativeSdk> $<TARGET_FILE_DIR:${PROJECT_NAME}>)

if (DXFCXX_INSTALL)
    include(GNUInstallDirs)
    set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/${PROJECT_NAME}PackConfig.cmake")
    set(CPACK_SOURCE_GENERATOR ZIP)
    set(CPACK_PACKAGE_VENDOR "Devexperts LLC")
    set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
    set(CPACK_PACKAGE_VERSION "${DXFCXX_PACKAGE_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${DXFCXX_VERSION}-${DXFCXX_TARGET_PLATFORM}${DXFCXX_PACKAGE_SUFFIX}")
    include(CPack)

    if (DXFCXX_INSTALL_LIB)
        #install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION bin OPTIONAL)
        install(TARGETS ${PROJECT_NAME})
        install(DIRECTORY "include" DESTINATION "." PATTERN "build" EXCLUDE)
        install(FILES "LICENSE" "README.md" DESTINATION ".")
    endif ()
endif ()