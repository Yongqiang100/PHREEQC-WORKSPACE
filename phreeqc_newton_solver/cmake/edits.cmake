# edits.cmake -- the five anchored upstream edits of the phreeqc_newton_solver
# overlay (decision OV-D1). Include this file to obtain the five edit variables
# and the function newton_solver_apply_edits(<engine_dir>). detach.cmake
# includes this file and shares the same edits.
#
# Each edit variable holds one string: <file>|<anchor>|<replacement>. The
# anchors and replacements are exactly the table of OV-D1, written with the
# CMake string escapes \t and \n. No element contains '|'. The edits are kept
# in five separate variables, not in one list, because anchors and replacements
# contain ';' and CMake lists are ';' separated.

set(NEWTON_SOLVER_EDIT_1
  "Phreeqc.h|#include \"global_structures.h\"\n\nclass Phreeqc|#include \"global_structures.h\"\n#include \"newton_solver_phreeqc_forward.h\" /* phreeqc_newton_solver edit 1 */\n\nclass Phreeqc")
set(NEWTON_SOLVER_EDIT_2
  "Phreeqc.h|\tint ineq(int kode);\n|\tint ineq(int kode);\n#include \"newton_solver_phreeqc_members.h\" /* phreeqc_newton_solver edit 2 */\n")
set(NEWTON_SOLVER_EDIT_3
  "model.cpp|int Phreeqc::\nineq(int in_kode)\n|int Phreeqc::\nineq_cl1_original(int in_kode) /* phreeqc_newton_solver edit 3 */\n")
set(NEWTON_SOLVER_EDIT_4
  "model.cpp|\tcl1(k, l, m, n, l_nklmd, l_n2d, &ineq_array[0],\n\t\t&l_kode, ineq_tol, &l_iter, &delta1[0], &res[0],\n\t\t&l_error, &cu[0], &iu[0], &is[0], FALSE);\n|\tif (!newton_solver_before_cl1(in_kode, k, l, m, n, &ineq_array[0], &delta1[0], &res[0], &l_kode, &l_iter, &l_error)) /* phreeqc_newton_solver edit 4 */\n\tcl1(k, l, m, n, l_nklmd, l_n2d, &ineq_array[0],\n\t\t&l_kode, ineq_tol, &l_iter, &delta1[0], &res[0],\n\t\t&l_error, &cu[0], &iu[0], &is[0], FALSE);\n\tnewton_solver_after_cl1(l_kode, &delta1[0]); /* phreeqc_newton_solver edit 4 */\n")
set(NEWTON_SOLVER_EDIT_5
  "Phreeqc.cpp|Phreeqc::~Phreeqc(void)\n{\n|Phreeqc::~Phreeqc(void)\n{\n\tnewton_solver_destroy_state(); /* phreeqc_newton_solver edit 5 */\n")

# The edit variable names, in order E1..E5. Names contain no ';'.
set(NEWTON_SOLVER_EDIT_NAMES NEWTON_SOLVER_EDIT_1 NEWTON_SOLVER_EDIT_2 NEWTON_SOLVER_EDIT_3 NEWTON_SOLVER_EDIT_4 NEWTON_SOLVER_EDIT_5)

# Path of SUPPORTED_UPSTREAM.txt, resolved at include time.
set(NEWTON_SOLVER_SUPPORTED_UPSTREAM "${CMAKE_CURRENT_LIST_DIR}/../SUPPORTED_UPSTREAM.txt")
set(NEWTON_SOLVER_SUPPORTED_RELEASES "phreeqc3 v3.9.0 (b0b3be767158ccc3322d2c816625cf470045e67e) and PhreeqcRM v3.9.0 (90b31ed8f2080dd957af6cd75edb9ce099d0914b)")

# Split one edit string into its three parts.
function(newton_solver_split_edit edit out_file out_anchor out_replacement)
  string(FIND "${edit}" "|" p1)
  string(SUBSTRING "${edit}" 0 ${p1} file_part)
  math(EXPR after1 "${p1} + 1")
  string(SUBSTRING "${edit}" ${after1} -1 rest)
  string(FIND "${rest}" "|" p2)
  if(p1 LESS 0 OR p2 LESS 0)
    message(FATAL_ERROR "phreeqc_newton_solver: internal error, malformed edit string")
  endif()
  string(SUBSTRING "${rest}" 0 ${p2} anchor_part)
  math(EXPR after2 "${p2} + 1")
  string(SUBSTRING "${rest}" ${after2} -1 replacement_part)
  set(${out_file} "${file_part}" PARENT_SCOPE)
  set(${out_anchor} "${anchor_part}" PARENT_SCOPE)
  set(${out_replacement} "${replacement_part}" PARENT_SCOPE)
endfunction()

# The supported SHA-256 of one edited file name, from SUPPORTED_UPSTREAM.txt.
function(newton_solver_supported_hash name out_hash)
  if(NOT EXISTS "${NEWTON_SOLVER_SUPPORTED_UPSTREAM}")
    message(FATAL_ERROR "phreeqc_newton_solver: SUPPORTED_UPSTREAM.txt not found at ${NEWTON_SOLVER_SUPPORTED_UPSTREAM}")
  endif()
  file(STRINGS "${NEWTON_SOLVER_SUPPORTED_UPSTREAM}" lines REGEX "^[^#]")
  set(found "")
  foreach(line ${lines})
    string(REGEX MATCH "^([0-9a-f]+)[ \t]+([^ \t]+)" match "${line}")
    if(CMAKE_MATCH_2 STREQUAL "${name}")
      set(found "${CMAKE_MATCH_1}")
    endif()
  endforeach()
  if(NOT found)
    message(FATAL_ERROR "phreeqc_newton_solver: ${name} is not listed in ${NEWTON_SOLVER_SUPPORTED_UPSTREAM}")
  endif()
  set(${out_hash} "${found}" PARENT_SCOPE)
endfunction()

# The distinct edited file names, in edit order.
function(newton_solver_edited_files out_files)
  set(files "")
  foreach(edit_name ${NEWTON_SOLVER_EDIT_NAMES})
    newton_solver_split_edit("${${edit_name}}" f a r)
    if(NOT f IN_LIST files)
      list(APPEND files "${f}")
    endif()
  endforeach()
  set(${out_files} "${files}" PARENT_SCOPE)
endfunction()

# newton_solver_apply_edits(<engine directory>) -- specification "edits.cmake",
# steps 1-4. Nothing is written unless every check passes.
function(newton_solver_apply_edits engine_dir)
  newton_solver_edited_files(files)

  # Step 1: already edited?
  file(READ "${engine_dir}/Phreeqc.h" phreeqc_h_text)
  if(phreeqc_h_text MATCHES "phreeqc_newton_solver edit")
    foreach(f ${files})
      set(orig "${engine_dir}/${f}.phreeqc_newton_solver.orig")
      if(NOT EXISTS "${orig}")
        message(FATAL_ERROR "phreeqc_newton_solver: ${engine_dir}/${f} carries the overlay edits but ${orig} is missing")
      endif()
      file(SHA256 "${orig}" orig_hash)
      newton_solver_supported_hash("${f}" supported)
      if(NOT orig_hash STREQUAL supported)
        message(FATAL_ERROR "phreeqc_newton_solver: ${orig} has SHA-256 ${orig_hash}, which is not a supported upstream version (supported: ${NEWTON_SOLVER_SUPPORTED_RELEASES})")
      endif()
      # Re-derive the edited text from the .orig and compare byte for byte.
      file(READ "${orig}" text)
      foreach(edit_name ${NEWTON_SOLVER_EDIT_NAMES})
        newton_solver_split_edit("${${edit_name}}" ef anchor replacement)
        if(ef STREQUAL f)
          string(REPLACE "${anchor}" "${replacement}" text "${text}")
        endif()
      endforeach()
      file(READ "${engine_dir}/${f}" current)
      if(NOT text STREQUAL current)
        message(FATAL_ERROR "phreeqc_newton_solver: ${engine_dir}/${f} does not equal the text derived from ${orig}; the edited file was modified by hand")
      endif()
    endforeach()
    message(STATUS "phreeqc_newton_solver: upstream edits already present")
    return()
  endif()

  # Step 2: supported version?
  foreach(f ${files})
    file(SHA256 "${engine_dir}/${f}" current_hash)
    newton_solver_supported_hash("${f}" supported)
    if(NOT current_hash STREQUAL supported)
      message(FATAL_ERROR "phreeqc_newton_solver: ${engine_dir}/${f} has SHA-256 ${current_hash}, which is not a supported upstream version. Supported releases: ${NEWTON_SOLVER_SUPPORTED_RELEASES}. Nothing was written.")
    endif()
  endforeach()

  # Step 3: dry run. Every anchor must occur exactly once in its file.
  set(eindex 0)
  foreach(edit_name ${NEWTON_SOLVER_EDIT_NAMES})
    math(EXPR eindex "${eindex} + 1")
    newton_solver_split_edit("${${edit_name}}" f anchor replacement)
    file(READ "${engine_dir}/${f}" text)
    string(FIND "${text}" "${anchor}" pos_forward)
    string(FIND "${text}" "${anchor}" pos_reverse REVERSE)
    if(pos_forward LESS 0 OR NOT pos_forward STREQUAL pos_reverse)
      message(FATAL_ERROR "phreeqc_newton_solver: edit E${eindex} for ${f}: its anchor does not occur exactly once in ${engine_dir}/${f}. Nothing was written.")
    endif()
  endforeach()

  # Step 4: apply. Copy each file to .orig once, then write the replaced text.
  foreach(f ${files})
    set(orig "${engine_dir}/${f}.phreeqc_newton_solver.orig")
    if(NOT EXISTS "${orig}")
      configure_file("${engine_dir}/${f}" "${orig}" COPYONLY)
    endif()
    file(READ "${orig}" text)
    set(eindex 0)
    foreach(edit_name ${NEWTON_SOLVER_EDIT_NAMES})
      math(EXPR eindex "${eindex} + 1")
      newton_solver_split_edit("${${edit_name}}" ef anchor replacement)
      if(ef STREQUAL f)
        string(REPLACE "${anchor}" "${replacement}" text "${text}")
        message(STATUS "phreeqc_newton_solver: edit E${eindex} applied to ${f}")
      endif()
    endforeach()
    file(WRITE "${engine_dir}/${f}" "${text}")
  endforeach()
endfunction()
