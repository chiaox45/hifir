# bench/CMakeLists.txt

add_executable(bench_hif bench_hif.cpp)
target_link_libraries(bench_hif PRIVATE hifir)
# examples/ is on the include path so bench sources can reuse examples/ headers
# (e.g. demo_utils.hpp), matching how examples/CMakeLists.txt wires includes.
# bench_hif.cpp itself does not include demo_utils.hpp: every typedef in it is
# hard typed to double, and its parser silently ignores unknown flags.
target_include_directories(bench_hif PRIVATE ${CMAKE_SOURCE_DIR}/examples)
target_compile_options(bench_hif PRIVATE $<$<CONFIG:Release>:-ffast-math>)
if(UNIX AND NOT APPLE)
    target_link_options(bench_hif PRIVATE -rdynamic)
endif()
