cmake_minimum_required(VERSION 3.25)
project(ECUPlatformApp VERSION 0.1.0 LANGUAGES CXX)

# Portable application host proof and unchanged Core. No GUI or hardware targets.
option(ECU_APP_BUILD_TESTS "Build offline Core, host and repository tests" ON)
option(ECU_APP_BUILD_HOST "Build the offline, hardware-free application host" ON)
option(ECU_APP_SANITIZERS "Enable ASan/UBSan on GCC or Clang" OFF)

if(ECU_APP_SANITIZERS)
  if(MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "ECU_APP_SANITIZERS requires GCC or Clang")
  endif()
  # Core is deliberately built without RTTI; the vptr sanitizer needs RTTI.
  add_compile_options(-fsanitize=address,undefined -fno-sanitize=vptr -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined -fno-sanitize=vptr)
endif()

function(ecu_app_strict target)
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive- /Zc:__cplusplus)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()

add_subdirectory(src/core_v2)

if(ECU_APP_BUILD_HOST)
  add_subdirectory(src/app_sdk)
  add_subdirectory(src/app_host)
  add_subdirectory(apps/host)
endif()

if(ECU_APP_BUILD_TESTS)
  enable_testing()
  find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
  add_subdirectory(tests)
endif()
