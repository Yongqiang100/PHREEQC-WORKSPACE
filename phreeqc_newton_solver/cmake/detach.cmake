# detach.cmake -- remove the phreeqc_newton_solver upstream edits (decision OV-D1).
# Usage: cmake -DTREE=<tree root> -P <overlay>/cmake/detach.cmake
#
# Restores every edited file from its .phreeqc_newton_solver.orig copy. All
# checks run before any file is written.

if(NOT DEFINED TREE)
  message(FATAL_ERROR "phreeqc_newton_solver detach: usage: cmake -DTREE=<tree root> -P detach.cmake")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/edits.cmake")

# Locate the engine directory of the tree.
if(EXISTS "${TREE}/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/model.cpp")
  set(engine_dir "${TREE}/src/IPhreeqcPhast/IPhreeqc/phreeqcpp")
elseif(EXISTS "${TREE}/src/model.cpp")
  set(engine_dir "${TREE}/src")
else()
  message(FATAL_ERROR "phreeqc_newton_solver detach: ${TREE} is neither a phreeqc3 tree (src/model.cpp) nor a PhreeqcRM tree (src/IPhreeqcPhast/IPhreeqc/phreeqcpp/model.cpp)")
endif()

newton_solver_edited_files(files)

# Check every .orig before writing anything.
foreach(f ${files})
  set(orig "${engine_dir}/${f}.phreeqc_newton_solver.orig")
  if(NOT EXISTS "${orig}")
    message(FATAL_ERROR "phreeqc_newton_solver detach: ${orig} not found; nothing was written")
  endif()
  file(SHA256 "${orig}" orig_hash)
  newton_solver_supported_hash("${f}" supported)
  if(NOT orig_hash STREQUAL supported)
    message(FATAL_ERROR "phreeqc_newton_solver detach: ${orig} has SHA-256 ${orig_hash}, which is not a supported upstream version (supported: ${NEWTON_SOLVER_SUPPORTED_RELEASES}); nothing was written")
  endif()
endforeach()

# Restore each file from its .orig, then remove the .orig.
foreach(f ${files})
  set(orig "${engine_dir}/${f}.phreeqc_newton_solver.orig")
  configure_file("${orig}" "${engine_dir}/${f}" COPYONLY)
  file(REMOVE "${orig}")
endforeach()

# Confirm the restored hashes.
foreach(f ${files})
  file(SHA256 "${engine_dir}/${f}" restored_hash)
  newton_solver_supported_hash("${f}" supported)
  if(NOT restored_hash STREQUAL supported)
    message(FATAL_ERROR "phreeqc_newton_solver detach: after restore, ${engine_dir}/${f} has SHA-256 ${restored_hash}, expected ${supported}")
  endif()
  message(STATUS "phreeqc_newton_solver detach: restored ${engine_dir}/${f}")
endforeach()
