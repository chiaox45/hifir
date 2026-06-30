# libhifir/tests/CMakeLists.txt

file(GLOB HIFIR_C_TESTS CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "test_*.c")

foreach(_src ${HIFIR_C_TESTS})
    string(REPLACE ".c" "" _name ${_src})
    add_executable(${_name} ${_src})
    target_link_libraries(${_name} PRIVATE hifir_i32_shared m)
    target_include_directories(${_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
    # Build-tree RPATH for libhifir_i32.so discovery
    set_target_properties(${_name} PROPERTIES
        BUILD_RPATH "$ORIGIN/.."
        INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
    )
    add_test(NAME ${_name} COMMAND ${_name} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    set_tests_properties(${_name} PROPERTIES LABELS "c")
endforeach()
