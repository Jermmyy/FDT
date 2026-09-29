cmake_minimum_required(VERSION 3.17)
project(fed_terminal LANGUAGES CXX)

add_definitions(-DNOMINMAX)

# -- Global Settings --
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -- Cache overrides --
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL "" FORCE)

# -- Paths --

set(ULTRALIGHT_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ultralight-sdk")
set(UL_INCLUDE_DIR "${ULTRALIGHT_SDK_DIR}/include")
set(UL_LIB_DIR "${ULTRALIGHT_SDK_DIR}/lib")
set(UL_BIN_DIR "${ULTRALIGHT_SDK_DIR}/bin")

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
set(IMPLOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/implot")
set(PUGIXML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pugixml")

# -- Dependencies --

find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ixwebsocket CONFIG REQUIRED)
find_package(Boost REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)

include(FetchContent)

FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.10.4
)
FetchContent_MakeAvailable(cpr)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
    GIT_TAG        VER-2-13-3
)
FetchContent_MakeAvailable(freetype)

FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(entt)

FetchContent_Declare(
    tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
)
FetchContent_MakeAvailable(tracy)

FetchContent_Declare(
    flatbuffers
    GIT_REPOSITORY https://github.com/google/flatbuffers.git
    GIT_TAG        v24.3.25
)
set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FLATBUFFERS_INSTALL OFF CACHE BOOL "" FORCE)
set(FLATBUFFERS_BUILD_FLATC ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(flatbuffers)

set(SCHEMA_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../schemas/Message.fbs")
set(GENERATED_HEADER "${CMAKE_CURRENT_BINARY_DIR}/Message_generated.h")

add_custom_command(
    OUTPUT ${GENERATED_HEADER}
    COMMAND $<TARGET_FILE:flatc> --cpp -o ${CMAKE_CURRENT_BINARY_DIR} ${SCHEMA_FILE}
    DEPENDS ${SCHEMA_FILE} flatc
    COMMENT "Compiling FlatBuffers schema: ${SCHEMA_FILE}"
)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/misc/freetype/imgui_freetype.cpp
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
    ${PUGIXML_DIR}/src/pugixml.cpp
)

# --- Executable ---

add_executable(fed_terminal WIN32
    src/main.cpp
    src/api/RateClient.cpp
    src/api/SecClient.cpp
    src/api/FedClient.cpp
    src/config/RateConfigs.cpp
    src/panels/RatePanel.cpp
    src/panels/SecfilingPanel.cpp
    src/panels/FedPanel.cpp
    src/rendering/filing_renderer.cpp
    src/rendering/tile_renderer.cpp
    src/ui/Commandbar.cpp
    src/core/RegisterPanels.cpp
    ${tracy_SOURCE_DIR}/public/TracyClient.cpp
    ${IMGUI_SOURCES}
    "src/panels/AlertFeedPanel.cpp"
    "src/core/WatchlistManager.cpp"
    "src/panels/WatchlistPanel.cpp"
    "src/rss/RSSClient.cpp"
    "src/rss/RSSPoller.cpp"
    "src/panels/PplTrackerPanel.cpp"
    "src/logging/NetworkInspector.cpp"
    "src/panels/HelpPanel.cpp"
    "src/custom/keynav.cpp"
    "src/api/EquityProvider.cpp"
    "src/panels/FocusPanel.cpp"
    "src/panels/ListDisplayPanel.cpp"
    "src/core/commands/WatchlistCommands.cpp"
    "src/core/PanelRegistry.cpp"
    "src/core/CommandRegistry.cpp"
    "src/panels/ChatPanel.cpp"
    "src/networking/NetworkClient.cpp"
    "src/networking/NetworkClient.hpp"
    "src/config/FedReleaseCatalog.hpp"
    ${GENERATED_HEADER})

if(MSVC)
    # Strip CMake's default /GR flag to avoid override warnings, theres probably a better way to do this but idk :D
    string(REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    target_compile_options(fed_terminal PRIVATE /GR-)
endif()

target_compile_features(fed_terminal PRIVATE cxx_std_20)

target_compile_definitions(fed_terminal PRIVATE 
    NOMINMAX 
    IMGUI_ENABLE_FREETYPE
    # TRACY_ENABLE # Uncomment this if you want to run profiler
)

target_include_directories(fed_terminal PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMGUI_DIR}/misc/freetype
    ${IMPLOT_DIR}
    ${PUGIXML_DIR}/src
    ${UL_INCLUDE_DIR}
    ${tracy_SOURCE_DIR}/public
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_BINARY_DIR}
)

target_compile_definitions(fed_terminal PRIVATE IMGUI_ENABLE_FREETYPE)

target_link_libraries(fed_terminal PRIVATE
    cpr::cpr
    nlohmann_json::nlohmann_json
    d3d11
    dxgi
    d3dcompiler
    ixwebsocket::ixwebsocket
    ZLIB::ZLIB
    OpenSSL::SSL
    OpenSSL::Crypto
    bcrypt
    freetype
    EnTT::EnTT
    flatbuffers
    unofficial-sodium::sodium
    Boost::boost
    "${UL_LIB_DIR}/Ultralight.lib"
    "${UL_LIB_DIR}/UltralightCore.lib"
    "${UL_LIB_DIR}/WebCore.lib"
    "${UL_LIB_DIR}/AppCore.lib"
)

# --- Post-Build: Copy Ultralight DLLs and Resources ---

add_custom_command(TARGET fed_terminal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${UL_BIN_DIR}"
        "$<TARGET_FILE_DIR:fed_terminal>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ULTRALIGHT_SDK_DIR}/resources"
        "$<TARGET_FILE_DIR:fed_terminal>/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/src/fonts"
        "$<TARGET_FILE_DIR:fed_terminal>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config.local"
        "$<TARGET_FILE_DIR:fed_terminal>/config.local"

    COMMENT "Grafting Ultralight DLLs and Resources into build folder..."
)