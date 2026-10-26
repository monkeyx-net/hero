# Native SDL2 H.E.R.O. (C64 version) for Linux handhelds and desktops (aarch64,
# x86_64), Windows (MinGW), macOS and the web (Emscripten). See PROGRESS.md.
cmake_minimum_required(VERSION 3.13)
project(hero VERSION 0.1.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
# The game must stay bit-exact across architectures: no -ffast-math.
add_compile_options(-Wall -Wextra)

option(HERO_SANITIZE "Build with ASan/UBSan" OFF)
if(HERO_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_library(sdl2 INTERFACE)
if(EMSCRIPTEN)
  # Emscripten's own SDL2 port
  target_compile_options(sdl2 INTERFACE -sUSE_SDL=2)
  target_link_options(sdl2 INTERFACE -sUSE_SDL=2)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  target_link_libraries(sdl2 INTERFACE PkgConfig::SDL2)
endif()

# The game: translated logic, C64 hardware model, VIC/SID, tape loader.
# (SDL is used only for the main-thread coroutine's thread and semaphores.)
add_library(hero_core STATIC
  src/hero_main.c
  src/hero_irq.c
  src/hero_screen.c
  src/hero_play.c
  src/hero_events.c
  src/hero_panel.c
  src/hero_sound.c
  src/hero_util.c
  src/hw.c
  src/vic.c
  src/sid.c
  src/font.c
  src/render_hires.c
  src/tape.c
  src/png.c
)
target_include_directories(hero_core PUBLIC src)
target_compile_definitions(hero_core PUBLIC HERO_VERSION="${PROJECT_VERSION}")
target_link_libraries(hero_core PUBLIC sdl2 m)

add_executable(hero src/main.c src/platform_sdl.c)
target_link_libraries(hero PRIVATE hero_core)
# Development builds also look for H.E.R.O.tap in the source tree; releases look
# next to the program, in the current directory, $HERO_TAP or --tap.
option(HERO_DEV_DATA "Fall back to the source tree's H.E.R.O.tap at run time" ON)
if(HERO_DEV_DATA)
  target_compile_definitions(hero PRIVATE HERO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
endif()

# The player's tape (never committed; see .gitignore).
set(HERO_TAP "${CMAKE_SOURCE_DIR}/H.E.R.O.tap" CACHE FILEPATH "Your C64 H.E.R.O. tape image (.tap)")

# Web build: emcmake cmake -S . -B build-web && cmake --build build-web --target serve
# writes <build>/web/index.{html,js,wasm} and serves it on localhost. The tape is
# never bundled; HERO_WEB_TAP can add the user's own for local play only (do not
# publish such a build). Without it the page shows the no-tape screen.
if(EMSCRIPTEN)
  set(HERO_WEB_TAP "" CACHE FILEPATH "Bundle this H.E.R.O.tap for LOCAL testing only")
  set(HERO_WEB_PORT 8000 CACHE STRING "Port for the serve target")
  set(web_preload)
  if(HERO_WEB_TAP)
    list(APPEND web_preload "SHELL:--preload-file '${HERO_WEB_TAP}@/H.E.R.O.tap'")
    message(WARNING "Web build bundles ${HERO_WEB_TAP}: for local use only, do not publish it")
  endif()
  set(shell "${CMAKE_SOURCE_DIR}/src/web/shell.html")
  set_target_properties(hero PROPERTIES
    OUTPUT_NAME index SUFFIX .html
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/web"
    LINK_DEPENDS "${shell}")
  # the main-thread coroutine is an Emscripten fiber: Asyncify, no pthreads
  target_link_options(hero PRIVATE -sALLOW_MEMORY_GROWTH=1 -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536
    -sSTACK_SIZE=1048576 "SHELL:--shell-file '${shell}'" ${web_preload})

  find_program(PYTHON3_EXECUTABLE NAMES python3 python)
  add_custom_target(serve
    COMMAND ${CMAKE_COMMAND} -E echo "H.E.R.O.: http://localhost:${HERO_WEB_PORT}/"
    COMMAND ${PYTHON3_EXECUTABLE} -m http.server ${HERO_WEB_PORT} --bind 127.0.0.1
      --directory "${CMAKE_BINARY_DIR}/web"
    DEPENDS hero USES_TERMINAL)
  return()  # no tools, packages or tests for the web build
endif()

# Release archives: cmake --build build --target package_linux / package_portmaster /
# package_windows / package_macos (configure release builds with -DHERO_DEV_DATA=OFF).
# HERO_PACKAGE_TAP puts the user's own tape in the archives ("-with-tape" in
# the name) for their own devices only: never publish such an archive.
set(HERO_PACKAGE_TAP "" CACHE FILEPATH "Put this H.E.R.O.tap in the packages (PERSONAL use only)")
if(HERO_PACKAGE_TAP)
  message(WARNING "Packages include ${HERO_PACKAGE_TAP}: for your own devices only, do not publish them")
endif()
foreach(kind linux portmaster windows macos)
  add_custom_target(package_${kind}
    COMMAND ${CMAKE_COMMAND} -DKIND=${kind} -DSRC=${CMAKE_SOURCE_DIR} -DBIN=${CMAKE_BINARY_DIR}
      -DGAME=$<TARGET_FILE:hero> -DARCH=${CMAKE_SYSTEM_PROCESSOR} -DVERSION=${PROJECT_VERSION}
      "-DTAP=${HERO_PACKAGE_TAP}"
      -P ${CMAKE_SOURCE_DIR}/src/packaging/package.cmake
    DEPENDS hero
    COMMENT "Packaging ${kind} release")
endforeach()

# ---- development tools (reverse engineering / reference model) ----------------
option(HERO_TOOLS "Build the development tools in tools/" ON)
if(HERO_TOOLS AND NOT WIN32)
  add_library(cpu6502 STATIC tools/cpu6502.c tools/op6502.c)
  target_include_directories(cpu6502 PUBLIC tools)
  add_executable(tapdump tools/tapdump.c)
  add_executable(herounpack tools/herounpack.c)
  add_executable(dis6502 tools/dis6502.c tools/op6502.c)
  add_executable(c64run tools/c64run.c)
  target_link_libraries(c64run PRIVATE cpu6502)
  add_executable(c64view tools/c64view.c tools/c64sys.c src/vic.c)
  target_link_libraries(c64view PRIVATE cpu6502 sdl2)

  if(EXISTS "${HERO_TAP}")
    # the decrypted image the tools work on
    set(img "${CMAKE_BINARY_DIR}/c64mem.bin")
    add_custom_command(OUTPUT "${img}"
      COMMAND hero --tap "${HERO_TAP}" --dump-image "${img}"
      DEPENDS hero "${HERO_TAP}" COMMENT "Decrypting ${HERO_TAP}")
    add_custom_target(image DEPENDS "${img}")
    # disassembly with labels (+ coverage map if present)
    set(cov "${CMAKE_SOURCE_DIR}/extracted/cov.bin")
    add_custom_target(disasm
      COMMAND sh -c "$<TARGET_FILE:dis6502> -r 0 -l '${CMAKE_SOURCE_DIR}/docs/c64.labels' \
$([ -f '${cov}' ] && echo '-c ${cov}') '${img}' 8000 C000 > '${CMAKE_BINARY_DIR}/hero.asm'"
      COMMAND ${CMAKE_COMMAND} -E echo "wrote ${CMAKE_BINARY_DIR}/hero.asm"
      DEPENDS dis6502 image)
    # the ORIGINAL game on the reference model
    add_custom_target(run-ref COMMAND c64view "${img}" DEPENDS c64view image USES_TERMINAL)
  endif()
endif()

# ---- tests ---------------------------------------------------------------------
enable_testing()
add_executable(test_unit tests/test_unit.c tools/cpu6502.c tools/op6502.c)
target_link_libraries(test_unit PRIVATE hero_core)
add_test(NAME unit COMMAND test_unit)

if(EXISTS "${HERO_TAP}")
  add_test(NAME tape_load COMMAND hero --tap "${HERO_TAP}" --dump-image "${CMAKE_BINARY_DIR}/tape_test.bin")
  set_tests_properties(tape_load PROPERTIES LABELS tape)
  if(NOT WIN32)
    # every translated routine against the original 6502 code, and every screen
    add_executable(difftest tests/difftest.c tools/c64sys.c tools/cpu6502.c tools/op6502.c)
    target_link_libraries(difftest PRIVATE hero_core)
    add_test(NAME difftest_routines COMMAND difftest "${HERO_TAP}" 20 1)
    add_test(NAME difftest_screens COMMAND difftest "${HERO_TAP}" screens)
    set_tests_properties(difftest_routines difftest_screens PROPERTIES LABELS tape TIMEOUT 1800)
  endif()
  add_test(NAME replay COMMAND ${CMAKE_COMMAND} -DHERO=$<TARGET_FILE:hero> -DTAP=${HERO_TAP}
    -DSRC=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/tests/replay.cmake)
  set_tests_properties(replay PROPERTIES LABELS tape TIMEOUT 600)
  add_custom_target(replay_update
    COMMAND ${CMAKE_COMMAND} -DHERO=$<TARGET_FILE:hero> -DTAP=${HERO_TAP} -DSRC=${CMAKE_SOURCE_DIR}
      -DUPDATE=ON -P ${CMAKE_SOURCE_DIR}/tests/replay.cmake
    DEPENDS hero)
  message(STATUS "Tape found at ${HERO_TAP}: tape tests enabled")
else()
  message(STATUS "No tape at ${HERO_TAP}: tape tests disabled (put H.E.R.O.tap in the source folder)")
endif()
