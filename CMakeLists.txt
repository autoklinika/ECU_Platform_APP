cmake_minimum_required(VERSION 3.25)
project(ECUPlatformApp VERSION 0.1.0 LANGUAGES CXX)

# Standalone Core bootstrap. No legacy application, Qt, hardware or GUI targets.
option(ECU_APP_BUILD_TESTS "Build offline Core and repository verification tests" ON)
option(ECU_APP_SANITIZERS "Enable ASan/UBSan on GCC or Clang" OFF)

if(ECU_APP_SANITIZERS)
  if(MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "ECU_APP_SANITIZERS requires GCC or Clang")
  endif()
  # Core is deliberately built without RTTI; the vptr sanitizer needs RTTI.
  add_compile_options(-fsanitize=address,undefined -fno-sanitize=vptr -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined -fno-sanitize=vptr)
endif()

add_subdirectory(src/core_v2)

if(ECU_APP_BUILD_TESTS)
  enable_testing()
  find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
  add_subdirectory(tests)
endif()
