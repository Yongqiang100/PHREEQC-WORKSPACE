# attach.cmake -- entry point of the phreeqc_newton_solver overlay (decision OV-D3).
# Use: cmake -S <tree> -B <build> -DCMAKE_PROJECT_INCLUDE=<overlay>/cmake/attach.cmake

# 1. Guards (OV-D3).
if(NOT CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  return() # nested project(): PhreeqcRM Tests/, fetched content
endif()
include_guard(GLOBAL)
if(CMAKE_VERSION VERSION_LESS 3.19)
  message(FATAL_ERROR "phreeqc_newton_solver requires CMake 3.19 or later (cmake_language(DEFER)); this is ${CMAKE_VERSION}")
endif()

# 2. Tree detection. The flat PhreeqcRM layout is checked first: it is the most specific.
get_filename_component(NEWTON_SOLVER_OVERLAY_DIR "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)
if(EXISTS "${CMAKE_SOURCE_DIR}/src/phreeqc/model.cpp" AND EXISTS "${CMAKE_SOURCE_DIR}/src/phreeqcrm/PhreeqcRM.h")
  # Flat PhreeqcRM tree (phreeqcrm-flat): engine in src/phreeqc, shared helpers in src/phreeqc_common.
  set(NEWTON_SOLVER_ENGINE_DIR "${CMAKE_SOURCE_DIR}/src/phreeqc")
  set(NEWTON_SOLVER_COMMON_DIR "${CMAKE_SOURCE_DIR}/src/phreeqc_common")
  set(NEWTON_SOLVER_TARGET PhreeqcRM)
  set(NEWTON_SOLVER_LINK_KEYWORD PUBLIC)
  set(NEWTON_SOLVER_TREE_KIND rm)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/model.cpp")
  set(NEWTON_SOLVER_ENGINE_DIR "${CMAKE_SOURCE_DIR}/src/IPhreeqcPhast/IPhreeqc/phreeqcpp")
  set(NEWTON_SOLVER_COMMON_DIR "${NEWTON_SOLVER_ENGINE_DIR}/common")
  set(NEWTON_SOLVER_TARGET PhreeqcRM)
  set(NEWTON_SOLVER_LINK_KEYWORD PUBLIC)
  set(NEWTON_SOLVER_TREE_KIND rm)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/src/model.cpp")
  set(NEWTON_SOLVER_ENGINE_DIR "${CMAKE_SOURCE_DIR}/src")
  set(NEWTON_SOLVER_COMMON_DIR "${NEWTON_SOLVER_ENGINE_DIR}/common")
  set(NEWTON_SOLVER_TARGET phreeqc)
  set(NEWTON_SOLVER_LINK_KEYWORD NONE)
  set(NEWTON_SOLVER_TREE_KIND phreeqc3)
else()
  message(FATAL_ERROR "phreeqc_newton_solver: ${CMAKE_SOURCE_DIR} is none of the supported trees: a phreeqc3 tree (src/model.cpp), a PhreeqcRM tree (src/IPhreeqcPhast/IPhreeqc/phreeqcpp/model.cpp), or a flat PhreeqcRM tree (src/phreeqc/model.cpp with src/phreeqcrm/PhreeqcRM.h)")
endif()

# 3. Default-backend option (decision OV-D7 (b)): an overlay build with no
# environment variables must reproduce upstream PHREEQC exactly, so the default
# is cl1. Validated before any edit is written: an invalid value writes nothing.
set(NEWTON_SOLVER_DEFAULT_BACKEND "cl1" CACHE STRING "Default Newton linear-solver backend: cl1 or lapack")
if(NOT NEWTON_SOLVER_DEFAULT_BACKEND MATCHES "^(cl1|lapack)$")
  message(FATAL_ERROR "phreeqc_newton_solver: NEWTON_SOLVER_DEFAULT_BACKEND must be \"cl1\" or \"lapack\", got \"${NEWTON_SOLVER_DEFAULT_BACKEND}\"")
endif()

# 4. Upstream edits (OV-D1).
include("${CMAKE_CURRENT_LIST_DIR}/edits.cmake")
newton_solver_apply_edits("${NEWTON_SOLVER_ENGINE_DIR}")

option(NEWTON_SOLVER_BUILD_TESTS "Build the phreeqc_newton_solver unit tests" ON)

# Insert find_dependency(LAPACK) and find_dependency(Threads) into a generated
# PhreeqcRMConfig.cmake, before the include(...PhreeqcRMTargets.cmake) line
# (decision OV-D4). Idempotent: a file that already names LAPACK is left alone.
function(newton_solver_patch_config config_file)
  if(NOT EXISTS "${config_file}")
    return() # not a standalone build: no package config was generated
  endif()
  file(READ "${config_file}" text)
  if(text MATCHES "find_dependency\\(LAPACK\\)")
    message(STATUS "phreeqc_newton_solver: ${config_file} already declares the LAPACK and Threads dependencies")
    return()
  endif()
  string(FIND "${text}" "PhreeqcRMTargets.cmake" pos)
  if(pos LESS 0)
    message(FATAL_ERROR "phreeqc_newton_solver: ${config_file} does not include PhreeqcRMTargets.cmake; refusing to patch it")
  endif()
  string(SUBSTRING "${text}" 0 ${pos} head)
  string(FIND "${head}" "\n" line_start REVERSE)
  math(EXPR line_start "${line_start} + 1")
  string(SUBSTRING "${text}" 0 ${line_start} before)
  string(SUBSTRING "${text}" ${line_start} -1 after)
  file(WRITE "${config_file}" "${before}find_dependency(LAPACK)\nfind_dependency(Threads)\n${after}")
  message(STATUS "phreeqc_newton_solver: added find_dependency(LAPACK) and find_dependency(Threads) to ${config_file}")
endfunction()

# 5. Deferred attachment (OV-D3): runs after the engine target exists.
function(newton_solver_attach_target target link_keyword)
  set(ov "${NEWTON_SOLVER_OVERLAY_DIR}")
  target_sources(${target} PRIVATE
    "${ov}/src/glue/newton_solver_phreeqc_glue.cpp"
    "${ov}/src/core/newton_linear_solver_options.cpp"
    "${ov}/src/core/newton_linear_solver_state.cpp"
    "${ov}/src/lapack/lapack_newton_solver.cpp"
    "${ov}/src/lapack/cl1_problem_check.cpp")
  target_include_directories(${target} PRIVATE
    "${ov}/src/glue" "${ov}/src/core" "${ov}/src/lapack")
  target_compile_definitions(${target} PRIVATE USE_NEWTON_LINEAR_SOLVER USE_LAPACK_NEWTON_SOLVER)
  if(NEWTON_SOLVER_DEFAULT_BACKEND STREQUAL "cl1")
    target_compile_definitions(${target} PRIVATE NEWTON_SOLVER_DEFAULT_BACKEND_CL1)
  endif()
  find_package(LAPACK REQUIRED)
  find_package(Threads REQUIRED)
  if(link_keyword STREQUAL "NONE")
    target_link_libraries(${target} LAPACK::LAPACK Threads::Threads)
  else()
    target_link_libraries(${target} ${link_keyword} LAPACK::LAPACK Threads::Threads)
  endif()

  # Unit tests (OV-D3): each test compiles the core and LAPACK sources itself.
  if(NEWTON_SOLVER_BUILD_TESTS)
    enable_testing()
    foreach(name newton_linear_solver_options lapack_newton_solver cl1_problem_check newton_linear_solver_state)
      add_executable(newton_solver_test_${name}
        "${ov}/tests/test_${name}.cpp"
        "${ov}/src/core/newton_linear_solver_options.cpp"
        "${ov}/src/core/newton_linear_solver_state.cpp"
        "${ov}/src/lapack/lapack_newton_solver.cpp"
        "${ov}/src/lapack/cl1_problem_check.cpp")
      target_compile_features(newton_solver_test_${name} PRIVATE cxx_std_11)
      target_compile_definitions(newton_solver_test_${name} PRIVATE USE_NEWTON_LINEAR_SOLVER USE_LAPACK_NEWTON_SOLVER)
      if(NEWTON_SOLVER_DEFAULT_BACKEND STREQUAL "cl1")
        target_compile_definitions(newton_solver_test_${name} PRIVATE NEWTON_SOLVER_DEFAULT_BACKEND_CL1)
      endif()
      target_include_directories(newton_solver_test_${name} PRIVATE
        "${ov}/src/core" "${ov}/src/lapack" "${NEWTON_SOLVER_COMMON_DIR}")
      target_link_libraries(newton_solver_test_${name} PRIVATE LAPACK::LAPACK Threads::Threads)
      add_test(NAME newton_solver.${name} COMMAND newton_solver_test_${name})
    endforeach()
  endif()

  # 6. Install and export (OV-D4): PhreeqcRM trees only.
  if(NEWTON_SOLVER_TREE_KIND STREQUAL "rm")
    newton_solver_patch_config("${CMAKE_BINARY_DIR}/PhreeqcRMConfig.cmake")
  endif()
endfunction()

cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR} CALL newton_solver_attach_target ${NEWTON_SOLVER_TARGET} ${NEWTON_SOLVER_LINK_KEYWORD})
