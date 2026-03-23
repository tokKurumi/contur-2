include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# Unit tests
file(GLOB CONTUR2_UNIT_TESTS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp
)

if(CONTUR2_UNIT_TESTS)
    add_executable(contur2_unit_tests ${CONTUR2_UNIT_TESTS})
    target_link_libraries(contur2_unit_tests PRIVATE contur2_lib GTest::gtest_main)
    gtest_discover_tests(contur2_unit_tests)
endif()

# Integration tests
file(GLOB CONTUR2_INTEGRATION_TESTS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/*.cpp
)

if(CONTUR2_INTEGRATION_TESTS)
    add_executable(contur2_integration_tests ${CONTUR2_INTEGRATION_TESTS})
    target_link_libraries(contur2_integration_tests PRIVATE contur2_lib GTest::gtest_main)
    gtest_discover_tests(contur2_integration_tests)
endif()
