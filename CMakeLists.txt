cmake_minimum_required(VERSION 3.14)
project(SktechTime CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


include_directories(${CMAKE_SOURCE_DIR}/src)

add_executable(main
src/main.cc
        src/utils/core.cc
        src/optimizer/OLDCOptimizer.cc
        src/experiment/JitterControlExperiment.cc
        src/optimizer/JitterSketchOptimizer.cc
        src/utils/BOBHash.cc
        src/experiment/testing.cc
        src/sketch/JitterSketchS1Opt.cc)
