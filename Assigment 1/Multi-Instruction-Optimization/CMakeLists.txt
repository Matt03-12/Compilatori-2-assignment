cmake_minimum_required(VERSION 3.13)
project(MultiInstOpt)

find_package(ZLIB REQUIRED)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "LLVM version: ${LLVM_PACKAGE_VERSION}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_library(MultiInstOptPass MODULE MultiInstOpt2.cpp)

set_target_properties(MultiInstOptPass PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
)

llvm_map_components_to_libnames(llvm_libs core support)

target_link_libraries(MultiInstOptPass PRIVATE ${llvm_libs})
