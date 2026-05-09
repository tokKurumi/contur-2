include(GoogleTest)
find_package(GTest REQUIRED)

set(CONTUR2_TEST_MAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp)

# ---------------------------------------------------------------------------
# Native test fixtures
# ---------------------------------------------------------------------------
# On POSIX hosts (Linux/macOS) we compile a tiny hello.c into a real ELF/Mach-O
# binary that NativeEngine integration tests launch via fork/exec. On Windows
# this step is skipped — the Windows tests use built-in system binaries
# (hostname.exe, where.exe) which are always available.
#
# The path of the produced binary (or an empty string) is exposed to tests
# through a generated header `native_test_paths.h`.
set(CONTUR2_NATIVE_HELLO_BINARY "")

if(UNIX AND NOT APPLE)
    # Find a C compiler. On Linux x86 hosts the default toolchain (gcc or clang)
    # is sufficient; we don't need C++ here, only freestanding C.
    find_program(CONTUR2_NATIVE_CC NAMES clang gcc cc)
    if(CONTUR2_NATIVE_CC)
        set(CONTUR2_NATIVE_HELLO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/native_hello.c)
        set(CONTUR2_NATIVE_HELLO_BIN ${CMAKE_CURRENT_BINARY_DIR}/fixtures/native_hello)

        add_custom_command(
            OUTPUT  ${CONTUR2_NATIVE_HELLO_BIN}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/fixtures
            COMMAND ${CONTUR2_NATIVE_CC} -O0 -o ${CONTUR2_NATIVE_HELLO_BIN} ${CONTUR2_NATIVE_HELLO_SRC}
            DEPENDS ${CONTUR2_NATIVE_HELLO_SRC}
            COMMENT "Compiling native hello fixture for NativeEngine integration tests"
            VERBATIM
        )
        add_custom_target(contur2_native_hello_fixture
            DEPENDS ${CONTUR2_NATIVE_HELLO_BIN}
        )
        set(CONTUR2_NATIVE_HELLO_BINARY "${CONTUR2_NATIVE_HELLO_BIN}")
    else()
        message(STATUS "Contur2: no C compiler found for native fixtures; integration tests will skip the C hello demo")
    endif()
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/native_test_paths.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/native_test_paths.h
    @ONLY
)

# Kernel unit tests  (no TUI dependency)
file(GLOB CONTUR2_UNIT_TESTS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp
)
list(FILTER CONTUR2_UNIT_TESTS EXCLUDE REGEX ".*/test_tui[^/]*\\.cpp$")

if(CONTUR2_UNIT_TESTS)
    add_executable(contur2_unit_tests ${CONTUR2_TEST_MAIN_SOURCE} ${CONTUR2_UNIT_TESTS})
    target_link_libraries(contur2_unit_tests PRIVATE contur2_lib gtest::gtest)
    gtest_discover_tests(contur2_unit_tests)
endif()

# Kernel integration tests  (no TUI dependency)
file(GLOB CONTUR2_INTEGRATION_TESTS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/*.cpp
)
list(FILTER CONTUR2_INTEGRATION_TESTS EXCLUDE REGEX ".*/test_tui[^/]*\\.cpp$")

if(CONTUR2_INTEGRATION_TESTS)
    add_executable(contur2_integration_tests ${CONTUR2_TEST_MAIN_SOURCE} ${CONTUR2_INTEGRATION_TESTS})
    target_link_libraries(contur2_integration_tests PRIVATE contur2_lib gtest::gtest)
    target_include_directories(contur2_integration_tests PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/generated
    )
    if(TARGET contur2_native_hello_fixture)
        add_dependencies(contur2_integration_tests contur2_native_hello_fixture)
    endif()
    gtest_discover_tests(contur2_integration_tests)
endif()

if(TARGET contur2_tui)
    # TUI unit tests  (depend on contur2_tui, which transitively pulls contur2_lib)
    file(GLOB CONTUR2_TUI_UNIT_TESTS CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_tui*.cpp
    )

    if(CONTUR2_TUI_UNIT_TESTS)
        add_executable(contur2_tui_unit_tests ${CONTUR2_TEST_MAIN_SOURCE} ${CONTUR2_TUI_UNIT_TESTS})
        target_link_libraries(contur2_tui_unit_tests PRIVATE contur2_tui gtest::gtest)
        gtest_discover_tests(contur2_tui_unit_tests)
    endif()

    # TUI integration tests  (depend on contur2_tui)
    file(GLOB CONTUR2_TUI_INTEGRATION_TESTS CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/integration/test_tui*.cpp
    )

    if(CONTUR2_TUI_INTEGRATION_TESTS)
        add_executable(contur2_tui_integration_tests ${CONTUR2_TEST_MAIN_SOURCE} ${CONTUR2_TUI_INTEGRATION_TESTS})
        target_link_libraries(contur2_tui_integration_tests PRIVATE contur2_tui gtest::gtest)
        gtest_discover_tests(contur2_tui_integration_tests)
    endif()
endif()
