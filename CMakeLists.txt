cmake_minimum_required(VERSION 3.20)
message(STATUS "[tcxIME] Loading addon...")

# Collect source files
if(APPLE)
    file(GLOB_RECURSE TCX_IME_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    )
else()
    file(GLOB_RECURSE TCX_IME_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    )
endif()

add_library(tcxIME STATIC ${TCX_IME_SOURCES})
target_include_directories(tcxIME PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(tcxIME PUBLIC TrussC)

if(APPLE AND NOT EMSCRIPTEN)
    # Enable ARC for .mm files (required for weak properties in tcxIMEView)
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tcxIMEView.mm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tcxIME_mac.mm
        PROPERTIES COMPILE_FLAGS "-fobjc-arc"
    )
    target_link_libraries(tcxIME PUBLIC
        "-framework Carbon"
    )
elseif(WIN32)
    target_link_libraries(tcxIME PUBLIC imm32)
endif()

message(STATUS "[tcxIME] Addon loaded")
