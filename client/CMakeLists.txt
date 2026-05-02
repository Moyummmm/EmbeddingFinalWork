cmake_minimum_required(VERSION 3.16)
project(embedding-client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# --- Qt 6 ---
find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets)

# --- nlohmann/json ---
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# --- Source files ---
add_executable(client
    main.cpp
    mainwindow.cpp
    registry_client.cpp
    p2p_server.cpp
    p2p_transfer.cpp
    protocol.cpp
)

target_link_libraries(client PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Widgets
    nlohmann_json::nlohmann_json
)

# --- Platform-specific settings ---
if(WIN32)
    # Prevent console window on Windows release builds
    set_target_properties(client PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>
    )

    # Automatically run windeployqt after building so the exe is
    # directly runnable (copies Qt DLLs next to the binary).
    get_target_property(_qt6_core_loc Qt6::Core IMPORTED_LOCATION)
    get_filename_component(_qt6_bin_dir "${_qt6_core_loc}" DIRECTORY)

    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${_qt6_bin_dir}"
    )

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET client POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:client>"
            COMMENT "windeployqt: copying Qt DLLs..."
        )
    else()
        message(WARNING "windeployqt not found — run it manually before launching client.exe")
    endif()
endif()
