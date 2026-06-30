# examples/CMakeLists.txt

file(GLOB HIFIR_EXAMPLE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/beginner/demo_*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/intermediate/demo_*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/advanced/demo_*.cpp"
)

foreach(_src ${HIFIR_EXAMPLE_SOURCES})
    get_filename_component(_name ${_src} NAME_WE)
    add_executable(${_name} ${_src})
    target_link_libraries(${_name} PRIVATE hifir)
    # Include examples/ for demo_utils.hpp AND examples/advanced/ for gmres.hpp
    target_include_directories(${_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/advanced
    )
    target_compile_options(${_name} PRIVATE $<$<CONFIG:Release>:-ffast-math>)
    if(UNIX AND NOT APPLE)
        target_link_options(${_name} PRIVATE -rdynamic)
    endif()
endforeach()
