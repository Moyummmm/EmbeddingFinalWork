cmake_minimum_required(VERSION 3.14)
project(embedding-server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

add_executable(server
    main.cpp
    protocol.cpp
    registry.cpp
    server.cpp
)

target_link_libraries(server PRIVATE nlohmann_json::nlohmann_json)
