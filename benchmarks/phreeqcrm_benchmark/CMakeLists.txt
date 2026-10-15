# Standalone chemistry benchmark for PhreeqcRM (Phase 2 Task 4).
# Configure against an installed PhreeqcRM:
#   cmake -S <this dir> -B <build dir> -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<PhreeqcRM install prefix>
cmake_minimum_required(VERSION 3.20)
project(phreeqcrm_chemistry_benchmark CXX)
find_package(PhreeqcRM REQUIRED CONFIG)
add_executable(phreeqcrm_chemistry_benchmark phreeqcrm_chemistry_benchmark.cpp)
target_compile_features(phreeqcrm_chemistry_benchmark PRIVATE cxx_std_11)
target_link_libraries(phreeqcrm_chemistry_benchmark PRIVATE PhreeqcRM::PhreeqcRM)
