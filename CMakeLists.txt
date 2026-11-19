cmake_minimum_required(VERSION 3.20)
project(TempleFrog VERSION 0.2.0 LANGUAGES C CXX)
set(GBARECOMP_GAME_VERSION "${PROJECT_VERSION}" CACHE STRING "Player-visible game build version" FORCE)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GBARECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/gbarecomp"
    CACHE PATH "Path to this game's gbarecomp checkout")
if(NOT EXISTS "${GBARECOMP_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "GBARECOMP_ROOT is not a gbarecomp checkout: ${GBARECOMP_ROOT}")
endif()

set(RECOMP_UI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/recomp-ui"
    CACHE PATH "Path to the shared recomp-ui checkout")
set(GBARECOMP_RUNTIME_UI_ROOT "${RECOMP_UI_ROOT}"
    CACHE PATH "recomp-ui root used by the live GBA runtime overlay")
set(GBARECOMP_ENABLE_MODS ON CACHE BOOL "Enable TempleFrog's mod catalog" FORCE)
set(GBARECOMP_EMBED_SRC_DIR OFF CACHE BOOL "Keep the build machine path out of TempleFrog.exe" FORCE)
set(GBARECOMP_ENABLE_HQ2X ON CACHE BOOL "Build the optional HQ2x presentation library" FORCE)

# Game modules (gbarecomp/docs/GAME_MODULES.md). ON: TempleFrog.exe carries no code made
# from the ROM or BIOS; the player's copies are recompiled and compiled into
# modules on their machine with the game kit (build/gamekit). Releases are built
# this way. OFF: the code from generated/ (tools/regen.ps1) is linked in, for
# quick work on the game itself.
option(TEMPLEFROG_GAME_MODULES "Build the game from the player's ROM at run time" ON)
set(GBARECOMP_GAME_MODULE ${TEMPLEFROG_GAME_MODULES} CACHE BOOL "" FORCE)

add_subdirectory("${GBARECOMP_ROOT}" gbarecomp_build EXCLUDE_FROM_ALL)

set(GAME_GENERATED_SOURCES)
if(NOT TEMPLEFROG_GAME_MODULES)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/recompiled.cpp")
        message(FATAL_ERROR "Stale monolithic generated/recompiled.cpp found. Run tools/regen.ps1.")
    endif()
    file(GLOB GAME_GENERATED_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/generated/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/generated/*.cpp")
    if(NOT GAME_GENERATED_SOURCES)
        message(FATAL_ERROR "No generated sources. Run tools/regen.ps1 with your ROM first, "
                            "or build with -DTEMPLEFROG_GAME_MODULES=ON.")
    endif()
endif()

add_executable(TempleFrog src/main.cpp src/gax_hq.cpp src/game_policy.cpp src/launch_checks.cpp
    src/game_build.cpp src/build_screen.cpp ${GAME_GENERATED_SOURCES})
if(TEMPLEFROG_GAME_MODULES)
    target_compile_definitions(TempleFrog PRIVATE TEMPLEFROG_GAME_MODULES=1)
    # The recompiler players run: one file, no compiler runtime DLLs beside it.
    if(MINGW)
        target_link_options(gba_recompile PRIVATE -static)
    elseif(UNIX)
        target_link_options(gba_recompile PRIVATE -static-libstdc++ -static-libgcc)
    endif()
    # The game kit next to the program (tools/make_gamekit.py): the recompiler,
    # the module SDK and the compiler (tools/zig_toolchain.py).
    if(WIN32)
        set(TEMPLEFROG_ZIG_PLATFORM windows)
    else()
        set(TEMPLEFROG_ZIG_PLATFORM linux)
    endif()
    set(TEMPLEFROG_ZIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/toolchain-cache/zig-${TEMPLEFROG_ZIG_PLATFORM}"
        CACHE PATH "Trimmed Zig for the game kit (python tools/zig_toolchain.py)")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_custom_target(templefrog_gamekit ALL
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/make_gamekit.py"
            --dest "$<TARGET_FILE_DIR:TempleFrog>/gamekit"
            --recompiler "$<TARGET_FILE:gba_recompile>"
            --zig "${TEMPLEFROG_ZIG_DIR}"
        DEPENDS gba_recompile
        COMMENT "Assembling the game kit"
        VERBATIM)
    add_dependencies(templefrog_gamekit TempleFrog)
endif()

set(GAME_RECOMPILED_SOURCES ${GAME_GENERATED_SOURCES})
list(FILTER GAME_RECOMPILED_SOURCES INCLUDE REGEX "/recompiled_[0-9]+\\.cpp$")
if(GAME_RECOMPILED_SOURCES AND NOT MSVC)
    set_source_files_properties(${GAME_RECOMPILED_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-O1;-g0")
endif()

target_include_directories(TempleFrog PRIVATE
    "${GBARECOMP_ROOT}/src/armv4t"
    "${GBARECOMP_ROOT}/src/gba"
    "${GBARECOMP_ROOT}/src/runtime"
    "${GBARECOMP_ROOT}/src/debug"
    src)

if(MSVC)
    target_link_libraries(TempleFrog PRIVATE
        gbarecomp_runtime gbarecomp_debug gbarecomp_gba gbarecomp_armv4t)
    target_link_options(TempleFrog PRIVATE /STACK:16777216)
else()
    target_link_libraries(TempleFrog PRIVATE
        "-Wl,--start-group"
        gbarecomp_runtime gbarecomp_debug gbarecomp_gba gbarecomp_armv4t
        "-Wl,--end-group")
    if(WIN32)
        target_link_options(TempleFrog PRIVATE -Wl,--stack,16777216)
    endif()
endif()

target_compile_definitions(TempleFrog PRIVATE
    GBARECOMP_GAME_VERSION="${GBARECOMP_GAME_VERSION}"
    GBARECOMP_DEFAULT_DEBUG_PORT=19920
    GBARECOMP_WINDOW_TITLE="TempleFrog"
    GBARECOMP_DEFAULT_GAME_CONFIG="game.toml")

# recomp-ui pre-boot launcher (ROM/BIOS picker, settings, controller binds)
# and the in-game overlay menu (Select+Start on a pad).
option(GBAGAME_RECOMP_UI "Build the recomp-ui launcher and overlay" ON)
if(GBAGAME_RECOMP_UI)
    include(${RECOMP_UI_ROOT}/recomp_ui.cmake)
    add_library(game_launcher_ui STATIC src/game_launcher_boot.cpp)
    target_include_directories(game_launcher_ui PRIVATE
        "${GBARECOMP_ROOT}/src/runtime"
        "${GBARECOMP_ROOT}/src/gba"
        src)
    if(GBARECOMP_ENABLE_MODS)
        target_compile_definitions(game_launcher_ui PRIVATE GBARECOMP_ENABLE_MODS=1)
    endif()
    recomp_target_launcher_ui(game_launcher_ui CONSOLE gba)
    find_package(SDL2 REQUIRED)
    target_link_libraries(game_launcher_ui PUBLIC SDL2::SDL2)
    if(WIN32)
        target_link_libraries(game_launcher_ui PUBLIC opengl32)
    endif()
    target_link_libraries(TempleFrog PRIVATE game_launcher_ui)
    target_sources(TempleFrog PRIVATE src/boxart.cpp)
    set_property(SOURCE src/main.cpp src/build_screen.cpp APPEND PROPERTY COMPILE_DEFINITIONS GBAGAME_RECOMP_UI)
    # The build screen (src/build_screen.cpp) draws with recomp-ui's Dear ImGui.
    target_include_directories(TempleFrog PRIVATE "${RECOMP_UI_ROOT}/src/third_party/imgui")
endif()

# Optional HQ2x library next to the exe (replaceable DLL; LGPL hqx).
if(COMMAND gbarecomp_stage_hq2x)
    gbarecomp_stage_hq2x(TempleFrog)
endif()
if(TARGET gbarecomp_hqx)
    # Trusted plugin behind the preloaded "Pixel Smoothing" HQ2x/3x/4x feature.
    target_sources(TempleFrog PRIVATE src/hqx_plugin.cpp)
endif()

# Preloaded mod catalog (HQ2x presentation toggle) staged next to the exe.
set(TEMPLEFROG_PRELOADED_MODS "${CMAKE_CURRENT_SOURCE_DIR}/mods/preloaded")
if(EXISTS "${TEMPLEFROG_PRELOADED_MODS}/packages")
    file(GLOB_RECURSE TEMPLEFROG_MOD_MANIFESTS CONFIGURE_DEPENDS
        "${TEMPLEFROG_PRELOADED_MODS}/packages/*.toml")
    set_property(TARGET TempleFrog APPEND PROPERTY LINK_DEPENDS ${TEMPLEFROG_MOD_MANIFESTS})
    add_custom_command(TARGET TempleFrog POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${TEMPLEFROG_PRELOADED_MODS}" "$<TARGET_FILE_DIR:TempleFrog>/mods"
        COMMENT "Staging TempleFrog's preloaded mod catalog")
endif()

# TempleFrog's own icon (original art in art/icon): embedded in the exe on
# Windows, and staged for shortcuts, the applications menu and the launcher.
if(WIN32)
    enable_language(RC)
    target_sources(TempleFrog PRIVATE src/templefrog.rc)
    set_property(SOURCE src/templefrog.rc APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/art/icon/templefrog.ico")
endif()
add_custom_command(TARGET TempleFrog POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:TempleFrog>/assets/img"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/art/icon/templefrog.ico" "$<TARGET_FILE_DIR:TempleFrog>/assets/img/icon.ico"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/art/icon/templefrog_256.png" "$<TARGET_FILE_DIR:TempleFrog>/assets/img/icon.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/art/icon/brand_game.tga" "$<TARGET_FILE_DIR:TempleFrog>/assets/img/brand_game.tga"
    COMMENT "Staging the TempleFrog icon")

# Menu fonts (chosen pairing A): Chewy titles, Fredoka body/bold. Staged
# under the names the in-game menu and launcher look for, with licences.
set(TEMPLEFROG_FONTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts")
set(TEMPLEFROG_FONT_BODY "${TEMPLEFROG_FONTS}/Fredoka/Fredoka-Medium.ttf")
set(TEMPLEFROG_FONT_BOLD "${TEMPLEFROG_FONTS}/Fredoka/Fredoka-SemiBold.ttf")
if(NOT EXISTS "${TEMPLEFROG_FONT_BODY}")
    set(TEMPLEFROG_FONT_BODY "${TEMPLEFROG_FONTS}/Fredoka/Fredoka-Variable.ttf")
endif()
if(NOT EXISTS "${TEMPLEFROG_FONT_BOLD}")
    set(TEMPLEFROG_FONT_BOLD "${TEMPLEFROG_FONTS}/Fredoka/Fredoka-Variable.ttf")
endif()
add_custom_command(TARGET TempleFrog POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:TempleFrog>/assets/fonts/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TEMPLEFROG_FONT_BODY}" "$<TARGET_FILE_DIR:TempleFrog>/assets/fonts/ui_body.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TEMPLEFROG_FONT_BOLD}" "$<TARGET_FILE_DIR:TempleFrog>/assets/fonts/ui_bold.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TEMPLEFROG_FONTS}/Chewy/Chewy-Regular.ttf" "$<TARGET_FILE_DIR:TempleFrog>/assets/fonts/ui_title.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TEMPLEFROG_FONTS}/Fredoka/OFL.txt" "$<TARGET_FILE_DIR:TempleFrog>/assets/fonts/licenses/Fredoka-OFL.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TEMPLEFROG_FONTS}/Chewy/LICENSE.txt" "$<TARGET_FILE_DIR:TempleFrog>/assets/fonts/licenses/Chewy-LICENSE.txt"
    COMMENT "Staging TempleFrog's menu fonts")

# Runtime DLLs for a double-clickable build (MinGW + SDL2).
if(MINGW)
    get_filename_component(TEMPLEFROG_MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(dll SDL2.dll libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
        if(EXISTS "${TEMPLEFROG_MINGW_BIN}/${dll}")
            add_custom_command(TARGET TempleFrog POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${TEMPLEFROG_MINGW_BIN}/${dll}" "$<TARGET_FILE_DIR:TempleFrog>")
        endif()
    endforeach()
endif()
