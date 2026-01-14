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
  common/crypto/DesCipher.cpp
  common/utils/Base64.cpp
  common/protocol/ErrorCode.cpp
  common/protocol/JsonLite.cpp
  common/protocol/Message.cpp
)

set(SERVER_SOURCES
  server/main.cpp
  server/core/CommandRouter.cpp
  server/core/ServerConfig.cpp
  server/handlers/AuthHandlers.cpp
  server/handlers/AdminHandlers.cpp
  server/handlers/BasicHandlers.cpp
  server/handlers/FileHandlers.cpp
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
add_executable(ServerDemo ${SERVER_SOURCES})
add_executable(Client ${CLIENT_SOURCES})

# Common include dir (if you later add headers under common/)
target_include_directories(Server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(ServerDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(Client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Demo-only compile define
target_compile_definitions(ServerDemo PRIVATE VULN_DEMO=1)

# ----------------------------
# Windows libs: Winsock2
# ----------------------------
if (WIN32)
  target_link_libraries(Server PRIVATE ws2_32)
  target_link_libraries(ServerDemo PRIVATE ws2_32)
  target_link_libraries(Client PRIVATE ws2_32)
endif()

# ----------------------------
# OpenSSL (MSYS2 UCRT64 provides it)
# ----------------------------
find_package(OpenSSL REQUIRED)
target_link_libraries(Server PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(ServerDemo PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(Client PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# ----------------------------
# Warnings (optional)
# ----------------------------
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(Server PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(ServerDemo PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(Client PRIVATE -Wall -Wextra -Wpedantic)
elseif (MSVC)
  target_compile_options(Server PRIVATE /W4)
  target_compile_options(ServerDemo PRIVATE /W4)
  target_compile_options(Client PRIVATE /W4)
endif()

# ----------------------------
# Demo build: disable security features for vulnerability demonstration
# ----------------------------
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT WIN32)
  # Disable stack protection, ASLR, DEP for demo (ELF only)
  target_compile_options(ServerDemo PRIVATE -fno-stack-protector)
  target_link_options(ServerDemo PRIVATE -z execstack -no-pie)
elseif (MINGW)
  # Disable stack protection, ASLR, DEP for demo (PE/COFF on MinGW)
  target_compile_options(ServerDemo PRIVATE -fno-stack-protector)
  target_link_options(ServerDemo PRIVATE -Wl,--disable-dynamicbase -Wl,--disable-nxcompat)
elseif (MSVC)
  # Disable stack protection, DEP for demo
  target_compile_options(ServerDemo PRIVATE /GS-)
  target_link_options(ServerDemo PRIVATE /NXCOMPAT:NO)
endif()
