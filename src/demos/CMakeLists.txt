# Shared demos library — optional utilities for demo executables
file(GLOB_RECURSE CONTUR2_DEMO_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

if(CONTUR2_DEMO_SOURCES)
    add_library(contur2_demos STATIC ${CONTUR2_DEMO_SOURCES})
else()
    add_library(contur2_demos INTERFACE)
endif()

if(CONTUR2_DEMO_SOURCES)
    target_include_directories(contur2_demos PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )
    target_link_libraries(contur2_demos PUBLIC contur2_lib)

    # Step-by-step mode in Debug builds
    target_compile_definitions(contur2_demos
        PRIVATE
            $<$<CONFIG:Debug>:CONTUR_STEP_MODE>
    )
else()
    target_include_directories(contur2_demos INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )
endif()

# Helper for per-demo executables.
function(contur2_add_demo demo_name)
    add_executable(${demo_name} ${ARGN})
    target_link_libraries(${demo_name}
        PRIVATE
            contur2_tui
            contur2_demos
            contur2_lib
    )
    target_include_directories(${demo_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_definitions(${demo_name}
        PRIVATE
            $<$<CONFIG:Debug>:CONTUR_STEP_MODE>
    )
endfunction()

# Auto-register demo subdirectories (skip folders starting with underscore).
file(GLOB DEMO_CMAKE_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*/CMakeLists.txt
)

foreach(DEMO_CMAKE_FILE IN LISTS DEMO_CMAKE_FILES)
    get_filename_component(DEMO_DIR ${DEMO_CMAKE_FILE} DIRECTORY)
    get_filename_component(DEMO_DIR_NAME ${DEMO_DIR} NAME)
    if(NOT DEMO_DIR_NAME MATCHES "^_")
        add_subdirectory(${DEMO_DIR_NAME})
    endif()
endforeach()
