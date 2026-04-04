include(GoogleTest)
find_package(GTest REQUIRED)

set(CONTUR2_TEST_MAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp)

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
