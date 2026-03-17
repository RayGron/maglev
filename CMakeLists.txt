cmake_minimum_required(VERSION 3.22)

project(maglev VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(nlohmann_json CONFIG REQUIRED)

set(MAGLEV_PLATFORM_DIR "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${MAGLEV_PLATFORM_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${MAGLEV_PLATFORM_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${MAGLEV_PLATFORM_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${MAGLEV_PLATFORM_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${MAGLEV_PLATFORM_DIR}")

set(
  MAGLEV_SOURCES
  cpp/src/contracts/contracts.cpp
  cpp/src/contracts/agent_contracts.cpp
  cpp/src/contracts/repository_contracts.cpp
  cpp/src/contracts/openai_contracts.cpp
  cpp/src/app/main.cpp
  cpp/src/runtime/config.cpp
  cpp/src/runtime/auth.cpp
  cpp/src/runtime/session.cpp
  cpp/src/runtime/execution.cpp
  cpp/src/runtime/logging.cpp
  cpp/src/gateway/gateway_common.cpp
  cpp/src/gateway/gateway.cpp
  cpp/src/gateway/mock_gateway.cpp
  cpp/src/gateway/openai_compat_gateway.cpp
  cpp/src/gateway/secure_gateway.cpp
  cpp/src/chat/chat_commands.cpp
  cpp/src/chat/chat_intent.cpp
  cpp/src/chat/chat_repository.cpp
  cpp/src/chat/chat_presenter.cpp
  cpp/src/chat/chat.cpp
  cpp/src/runtime/util.cpp
)

set(
  MAGLEV_HEADERS
  cpp/include/maglev/contracts/json_types.h
  cpp/include/maglev/contracts/repository_contracts.h
  cpp/include/maglev/contracts/agent_contracts.h
  cpp/include/maglev/contracts/openai_contracts.h
  cpp/include/maglev/contracts/contracts.h
  cpp/include/maglev/runtime/config.h
  cpp/include/maglev/runtime/auth.h
  cpp/include/maglev/gateway/agent_gateway.h
  cpp/include/maglev/runtime/session.h
  cpp/include/maglev/runtime/execution.h
  cpp/include/maglev/runtime/logging.h
  cpp/include/maglev/gateway/gateway.h
  cpp/include/maglev/gateway/mock_gateway.h
  cpp/include/maglev/gateway/openai_compat_gateway.h
  cpp/include/maglev/gateway/secure_gateway.h
  cpp/include/maglev/chat/chat.h
  cpp/include/maglev/runtime/util.h
  cpp/include/maglev/gateway/gateway_internal.h
  cpp/include/maglev/chat/chat_commands.h
  cpp/include/maglev/chat/chat_intent.h
  cpp/include/maglev/chat/chat_repository.h
  cpp/include/maglev/chat/chat_presenter.h
)

add_executable(
  maglev
  ${MAGLEV_SOURCES}
  ${MAGLEV_HEADERS}
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/cpp" PREFIX "cpp" FILES ${MAGLEV_SOURCES} ${MAGLEV_HEADERS})

target_compile_features(maglev PRIVATE cxx_std_20)
set_target_properties(maglev PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
  target_compile_definitions(maglev PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(maglev PRIVATE /W4 /permissive-)
else()
  target_compile_options(maglev PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_include_directories(
  maglev
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/cpp/include"
)

target_link_libraries(
  maglev
  PRIVATE
    nlohmann_json::nlohmann_json
)
