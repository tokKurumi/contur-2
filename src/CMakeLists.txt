cmake_minimum_required(VERSION 3.20)
project(contur2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CONTUR2_BUILD_TUI "Build TUI layer" ON)
option(CONTUR2_BUILD_APP "Build application executable" ON)

# Tracing (compile-time toggle)
option(CONTUR2_ENABLE_TRACING "Enable kernel tracing (Debug only)" OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CONTUR2_ENABLE_TRACING ON CACHE BOOL "" FORCE)
endif()

# contur2_lib — kernel static library (no TUI)
# Collects all .cpp files under contur/ EXCEPT the tui/ subdirectory.
file(GLOB_RECURSE CONTUR2_LIB_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/contur/*.cpp
)

# TUI is a separate layer — exclude it from the kernel library so that the
# kernel has zero UI dependencies and can be used headlessly.
list(FILTER CONTUR2_LIB_SOURCES EXCLUDE REGEX ".*/contur/tui/.*\\.cpp$")

# If no .cpp files exist yet, create a header-only INTERFACE target temporarily,
# then switch to STATIC once sources appear.
if(CONTUR2_LIB_SOURCES)
    add_library(contur2_lib STATIC ${CONTUR2_LIB_SOURCES})
else()
    # No sources yet — use an INTERFACE library so linking still works
    add_library(contur2_lib INTERFACE)
endif()

# Public include path: src/include/
# Usage: #include "contur/core/types.h"
if(CONTUR2_LIB_SOURCES)
    target_include_directories(contur2_lib PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    if(CONTUR2_ENABLE_TRACING)
        target_compile_definitions(contur2_lib PUBLIC CONTUR_TRACE_ENABLED)
    endif()
else()
    target_include_directories(contur2_lib INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    if(CONTUR2_ENABLE_TRACING)
        target_compile_definitions(contur2_lib INTERFACE CONTUR_TRACE_ENABLED)
    endif()
endif()

# contur2_tui — TUI layer (separate from kernel)
#
# Owns: tui/history_buffer, tui/kernel_read_model, tui/tui_controller,
#       tui/ftxui_renderer, tui/ftxui_app.
# Depends on: contur2_lib (kernel) via PUBLIC linkage, and FTXUI.
# Rule: contur2_lib must NEVER link or include contur2_tui.
if(CONTUR2_BUILD_TUI)
    # If Conan generators were produced next to the current binary dir,
    # prepend them so find_package(ftxui) can resolve without extra flags.
    if(CMAKE_BUILD_TYPE)
        set(CONTUR2_CONAN_GENERATORS_DIR "${CMAKE_BINARY_DIR}/build/${CMAKE_BUILD_TYPE}/generators")
        if(EXISTS "${CONTUR2_CONAN_GENERATORS_DIR}")
            list(PREPEND CMAKE_PREFIX_PATH "${CONTUR2_CONAN_GENERATORS_DIR}")
            list(PREPEND CMAKE_MODULE_PATH "${CONTUR2_CONAN_GENERATORS_DIR}")
            message(STATUS "Contur2: using Conan generators from ${CONTUR2_CONAN_GENERATORS_DIR}")
        endif()
    endif()

    find_package(ftxui REQUIRED)

    file(GLOB_RECURSE CONTUR2_TUI_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/contur/tui/*.cpp
    )

    if(CONTUR2_TUI_SOURCES)
        add_library(contur2_tui STATIC ${CONTUR2_TUI_SOURCES})
        target_include_directories(contur2_tui PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        )
        target_link_libraries(contur2_tui PUBLIC contur2_lib ftxui::ftxui)
    else()
        add_library(contur2_tui INTERFACE)
        target_include_directories(contur2_tui INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        )
        target_link_libraries(contur2_tui INTERFACE contur2_lib ftxui::ftxui)
    endif()
endif()

# Subdirectories
add_subdirectory(demos)
if(CONTUR2_BUILD_APP)
    if(NOT CONTUR2_BUILD_TUI)
        message(FATAL_ERROR "CONTUR2_BUILD_APP=ON requires CONTUR2_BUILD_TUI=ON")
    endif()
    add_subdirectory(app)
endif()

# Tests
option(CONTUR2_BUILD_TESTS "Build unit and integration tests" ON)
if(CONTUR2_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Doxygen Documentation
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    # Download doxygen-awesome-css theme
    include(FetchContent)
    FetchContent_Declare(doxygen-awesome-css
        URL https://github.com/jothepro/doxygen-awesome-css/archive/refs/tags/v2.4.2.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(doxygen-awesome-css)

    # Configure docs from a base Doxyfile plus an override file.
    set(DOXYGEN_AWESOME_DIR "${doxygen-awesome-css_SOURCE_DIR}")
    set(CONTUR2_DOXYFILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen/Doxyfile.in")
    set(CONTUR2_DOXYFILE_OVERRIDE_IN "${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen/Doxyfile.override.in")
    set(CONTUR2_DOXYFILE "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.docs")
    set(CONTUR2_DOXYFILE_OVERRIDE "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.override.docs")

    if(EXISTS "${CONTUR2_DOXYFILE_OVERRIDE_IN}")
        configure_file("${CONTUR2_DOXYFILE_OVERRIDE_IN}" "${CONTUR2_DOXYFILE_OVERRIDE}" @ONLY)
    else()
        file(WRITE "${CONTUR2_DOXYFILE_OVERRIDE}" "# Empty override Doxyfile\n")
    endif()
    configure_file("${CONTUR2_DOXYFILE_IN}" "${CONTUR2_DOXYFILE}" @ONLY)

    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} "${CONTUR2_DOXYFILE}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )

    set_property(TARGET docs PROPERTY SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen/Doxyfile.in"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen/Doxyfile.override.in"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen/mainpage.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen/getting_started.md"
    )

    message(STATUS "Doxygen documentation target 'docs' available")
    message(STATUS "  Theme: doxygen-awesome-css")
    message(STATUS "  Base Doxyfile: ${CONTUR2_DOXYFILE_IN}")
    message(STATUS "  Override Doxyfile: ${CONTUR2_DOXYFILE_OVERRIDE_IN}")
    message(STATUS "  Generated Doxyfile: ${CONTUR2_DOXYFILE}")
else()
    message(WARNING "Doxygen not found - documentation target will not be available")
endif()
