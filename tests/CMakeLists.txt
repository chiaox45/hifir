# tests/CMakeLists.txt — C++ unit tests via GTest

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
    FIND_PACKAGE_ARGS NAMES GTest
)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)
set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)
file(GLOB HIFIR_TEST_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "test_*.cpp")

foreach(_src ${HIFIR_TEST_SOURCES})
    string(REPLACE ".cpp" "" _name ${_src})
    add_executable(${_name} ${_src})
    target_link_libraries(${_name} PRIVATE hifir GTest::gtest GTest::gtest_main)
    target_include_directories(${_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(${_name} PRIVATE $<$<CONFIG:Release>:-ffast-math>)
    if(UNIX AND NOT APPLE)
        target_link_options(${_name} PRIVATE -rdynamic)
    endif()
    gtest_discover_tests(${_name} PROPERTIES LABELS "cpp")
endforeach()
