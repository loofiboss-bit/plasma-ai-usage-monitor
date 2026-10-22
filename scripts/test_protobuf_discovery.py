#!/usr/bin/env python3
"""Configure, generate, compile, link and run Protobuf through both discovery paths."""
from pathlib import Path
import subprocess
import tempfile

with tempfile.TemporaryDirectory(prefix="aiusage-protobuf-") as directory:
    root = Path(directory)
    (root / "smoke.proto").write_text('syntax = "proto3"; message Smoke { string value = 1; }\n')
    (root / "main.cpp").write_text('''#include "smoke.pb.h"
#include <string>
int main() {
    Smoke message;
    message.set_value("round-trip");
    std::string data;
    if (!message.SerializeToString(&data)) return 1;
    Smoke decoded;
    return !decoded.ParseFromString(data) || decoded.value() != "round-trip";
}
''')
    (root / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.22)
project(ProtobufDiscoverySmoke LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(protobuf_MODULE_COMPATIBLE ON)
if(DISCOVERY STREQUAL "CONFIG")
  find_package(Protobuf CONFIG QUIET)
  if(NOT Protobuf_FOUND)
    # Fedora ships only FindProtobuf. Exercise config discovery using a
    # controlled compatibility package backed by the installed real compiler.
    set(Protobuf_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config-fixture")
    find_package(Protobuf CONFIG REQUIRED)
    message(STATUS "Using controlled config fixture; native vendor config unavailable")
  endif()
else()
  find_package(Protobuf MODULE REQUIRED)
  # FindProtobuf supplies the compiler/library target, while modern Protobuf
  # also exposes transitive Abseil and utf8_range dependencies via pkg-config.
  # Keep older module-only installations working when no .pc file is shipped.
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(PROTOBUF_DEPS QUIET IMPORTED_TARGET protobuf)
  endif()
endif()
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS smoke.proto)
add_executable(smoke main.cpp ${PROTO_SRCS} ${PROTO_HDRS})
target_include_directories(smoke PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(smoke PRIVATE protobuf::libprotobuf)
if(TARGET PkgConfig::PROTOBUF_DEPS)
  target_link_libraries(smoke PRIVATE PkgConfig::PROTOBUF_DEPS)
endif()
''')
    fixture = root / "config-fixture"
    fixture.mkdir()
    (fixture / "ProtobufConfig.cmake").write_text(
        'include("${CMAKE_ROOT}/Modules/FindProtobuf.cmake")\n'
    )
    for mode in ("CONFIG", "MODULE"):
        build = root / mode.lower()
        subprocess.run(["cmake", "-S", str(root), "-B", str(build), f"-DDISCOVERY={mode}"], check=True)
        subprocess.run(["cmake", "--build", str(build), "--parallel", "2"], check=True)
        subprocess.run([str(build / "smoke")], check=True)
        print(f"Protobuf {mode}: configure/generate/link/round-trip PASS", flush=True)
