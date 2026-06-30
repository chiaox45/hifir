# libhifir/CMakeLists.txt

# Dual-int OBJECT libraries (compile once, use for both static and shared)
add_library(hifir_i32_objs OBJECT src/libhifir.cpp)
target_link_libraries(hifir_i32_objs PRIVATE hifir)
target_include_directories(hifir_i32_objs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(hifir_i32_objs PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(hifir_i64_objs OBJECT src/libhifir.cpp)
target_link_libraries(hifir_i64_objs PRIVATE hifir)
target_include_directories(hifir_i64_objs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(hifir_i64_objs PRIVATE LIBHIFIR_INT_SIZE=64)
set_target_properties(hifir_i64_objs PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Shared libraries
add_library(hifir_i32_shared SHARED $<TARGET_OBJECTS:hifir_i32_objs>)
target_link_libraries(hifir_i32_shared PRIVATE hifir LAPACK::LAPACK)
if(HIFIR_USE_OPENMP)
    target_link_libraries(hifir_i32_shared PRIVATE OpenMP::OpenMP_CXX)
endif()
set_target_properties(hifir_i32_shared PROPERTIES
    VERSION ${HIFIR_VERSION} SOVERSION ${HIFIR_VERSION_MAJOR}
    OUTPUT_NAME hifir_i32
)
target_compile_options(hifir_i32_shared PRIVATE $<$<CONFIG:Release>:-ffast-math>)
target_compile_definitions(hifir_i32_shared PRIVATE $<$<CONFIG:Debug>:HIF_DEBUG>)
add_library(hifir::hifir_i32 ALIAS hifir_i32_shared)

add_library(hifir_i64_shared SHARED $<TARGET_OBJECTS:hifir_i64_objs>)
target_link_libraries(hifir_i64_shared PRIVATE hifir LAPACK::LAPACK)
if(HIFIR_USE_OPENMP)
    target_link_libraries(hifir_i64_shared PRIVATE OpenMP::OpenMP_CXX)
endif()
set_target_properties(hifir_i64_shared PROPERTIES
    VERSION ${HIFIR_VERSION} SOVERSION ${HIFIR_VERSION_MAJOR}
    OUTPUT_NAME hifir_i64
)
target_compile_options(hifir_i64_shared PRIVATE $<$<CONFIG:Release>:-ffast-math>)
target_compile_definitions(hifir_i64_shared PRIVATE $<$<CONFIG:Debug>:HIF_DEBUG>)
add_library(hifir::hifir_i64 ALIAS hifir_i64_shared)

# Static libraries (hifir PRIVATE — C consumers don't need C++17; LAPACK/OpenMP PUBLIC — static consumers must resolve at link)
add_library(hifir_i32_static STATIC $<TARGET_OBJECTS:hifir_i32_objs>)
target_link_libraries(hifir_i32_static PRIVATE hifir)
target_link_libraries(hifir_i32_static PUBLIC LAPACK::LAPACK)
if(HIFIR_USE_OPENMP)
    target_link_libraries(hifir_i32_static PUBLIC OpenMP::OpenMP_CXX)
endif()
set_target_properties(hifir_i32_static PROPERTIES OUTPUT_NAME hifir_i32)
target_compile_options(hifir_i32_static PRIVATE $<$<CONFIG:Release>:-ffast-math>)
target_compile_definitions(hifir_i32_static PRIVATE $<$<CONFIG:Debug>:HIF_DEBUG>)
add_library(hifir::hifir_i32_static ALIAS hifir_i32_static)

add_library(hifir_i64_static STATIC $<TARGET_OBJECTS:hifir_i64_objs>)
target_link_libraries(hifir_i64_static PRIVATE hifir)
target_link_libraries(hifir_i64_static PUBLIC LAPACK::LAPACK)
if(HIFIR_USE_OPENMP)
    target_link_libraries(hifir_i64_static PUBLIC OpenMP::OpenMP_CXX)
endif()
set_target_properties(hifir_i64_static PROPERTIES OUTPUT_NAME hifir_i64)
target_compile_options(hifir_i64_static PRIVATE $<$<CONFIG:Release>:-ffast-math>)
target_compile_definitions(hifir_i64_static PRIVATE $<$<CONFIG:Debug>:HIF_DEBUG>)
add_library(hifir::hifir_i64_static ALIAS hifir_i64_static)

# -rdynamic for backtraces (Linux only — macOS doesn't support it)
if(UNIX AND NOT APPLE)
    target_link_options(hifir_i32_shared PRIVATE -rdynamic)
    target_link_options(hifir_i64_shared PRIVATE -rdynamic)
endif()
