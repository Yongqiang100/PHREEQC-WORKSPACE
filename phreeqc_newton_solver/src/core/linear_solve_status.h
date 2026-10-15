#pragma once
enum LinearSolveStatus {
    LINEAR_SOLVE_ACCEPTED = 0,       /* PETSc step accepted                                   */
    LINEAR_SOLVE_FALL_BACK_TO_CL1,   /* use the existing cl1 path for this Newton iteration   */
    LINEAR_SOLVE_NON_FINITE_INPUT    /* NaN or Inf in the input; in Phase 1A also falls back  */
};
