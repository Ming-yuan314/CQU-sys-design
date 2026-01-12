cmake_minimum_required(VERSION 3.20)

project(cs_remote
  VERSION 0.1.0
  LANGUAGES CXX
)

# ----------------------------
# C++ standard
# ----------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release if not specified (optional; you can remove this block)
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ----------------------------
# Output dirs (keep build tidy)
# ----------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ----------------------------
# Source lists
# ----------------------------
set(COMMON_SOURCES
  common/dummy.cpp
  common/net/SocketInit.cpp
  common/net/FramedIO.cpp
)

set(SERVER_SOURCES
  server/main.cpp
  ${COMMON_SOURCES}
)

set(CLIENT_SOURCES
  client/main.cpp
  ${COMMON_SOURCES}
)

# ----------------------------
# Targets
# ----------------------------
add_executable(Server ${SERVER_SOURCES})
add_executable(Client ${CLIENT_SOURCES})

# Common include dir (if you later add headers under common/)
target_include_directories(Server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(Client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ----------------------------
# Windows libs: Winsock2
# ----------------------------
if (WIN32)
  target_link_libraries(Server PRIVATE ws2_32)
  target_link_libraries(Client PRIVATE ws2_32)
endif()

# ----------------------------
# OpenSSL (MSYS2 UCRT64 provides it)
# ----------------------------
find_package(OpenSSL REQUIRED)
target_link_libraries(Server PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(Client PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# ----------------------------
# Warnings (optional)
# ----------------------------
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(Server PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(Client PRIVATE -Wall -Wextra -Wpedantic)
elseif (MSVC)
  target_compile_options(Server PRIVATE /W4)
  target_compile_options(Client PRIVATE /W4)
endif()
