cmake_minimum_required (VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)

project (CUDAEdgeDetection CXX CUDA)

set (CUDAEdgeDetection_SOURCE_DIR "${CMAKE_SOURCE_DIR}/Source")

# Enumerate source files.
file (GLOB_RECURSE CUDAEdgeDetection_SOURCES
    CONFIGURE_DEPENDS
    "${CUDAEdgeDetection_SOURCE_DIR}/*.c"
    "${CUDAEdgeDetection_SOURCE_DIR}/*.cc"
    "${CUDAEdgeDetection_SOURCE_DIR}/*.cpp"
    "${CUDAEdgeDetection_SOURCE_DIR}/*.cxx"
    "${CUDAEdgeDetection_SOURCE_DIR}/*.h"
    "${CUDAEdgeDetection_SOURCE_DIR}/*.hh"
    "${CUDAEdgeDetection_SOURCE_DIR}/*.hpp"
    "${CUDAEdgeDetection_SOURCE_DIR}/*.hxx"
    "${CUDAEdgeDetection_SOURCE_DIR}/*.inl"
    "${CUDAEdgeDetection_SOURCE_DIR}/*.cu")

source_group (TREE ${CUDAEdgeDetection_SOURCE_DIR} FILES ${CUDAEdgeDetection_SOURCES})

# Import dependencies.
find_package (MPI REQUIRED)
find_package (CUDAToolkit REQUIRED)

# Add executable target.
add_executable (CUDAEdgeDetection ${CUDAEdgeDetection_SOURCES})
target_precompile_headers (CUDAEdgeDetection PUBLIC "${CUDAEdgeDetection_SOURCE_DIR}/Pch.h")
set_target_properties(CUDAEdgeDetection PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
target_link_libraries (CUDAEdgeDetection PUBLIC MPI::MPI_CXX CUDA::cudart cuda.lib)
target_include_directories (CUDAEdgeDetection PUBLIC External/stb)
