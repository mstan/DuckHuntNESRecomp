cmake_minimum_required(VERSION 3.20)
project(DuckHuntRecomp C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(NESRECOMP_ROOT ${CMAKE_SOURCE_DIR}/nesrecomp)

include(${NESRECOMP_ROOT}/runner/runner.cmake)

list(APPEND CMAKE_PREFIX_PATH "${NESRECOMP_ROOT}/runner/external/SDL2/cmake")
find_package(SDL2 REQUIRED)

add_executable(DuckHuntRecomp
    ${NESRECOMP_RUNNER_SOURCES}
    extras.c
    nes_snapshot.c
    verify_mode.c
    generated/duck-hunt_full.c
    generated/duck-hunt_dispatch.c
)

target_include_directories(DuckHuntRecomp PRIVATE
    ${NESRECOMP_RUNNER_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(DuckHuntRecomp PRIVATE ${SDL2_LIBRARIES})

target_compile_definitions(DuckHuntRecomp PRIVATE RECOMP_STACK_TRACKING)

if(WIN32)
    target_link_libraries(DuckHuntRecomp PRIVATE ws2_32)
    add_custom_command(TARGET DuckHuntRecomp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${NESRECOMP_ROOT}/runner/external/SDL2/lib/x64/SDL2.dll"
            "$<TARGET_FILE_DIR:DuckHuntRecomp>"
    )
endif()

if(MSVC)
    target_compile_options(DuckHuntRecomp PRIVATE /W3 /WX-)
    target_compile_options(DuckHuntRecomp PRIVATE /wd4102)
endif()

# ---- Pre-boot GUI launcher (recomp-ui / Dear ImGui) ----
include(${CMAKE_SOURCE_DIR}/recomp-ui/recomp_ui.cmake)
recomp_target_launcher_ui(DuckHuntRecomp BOXART ${CMAKE_SOURCE_DIR}/boxart.tga)
target_compile_definitions(DuckHuntRecomp PRIVATE NESRECOMP_GAME_ZAPPER)
