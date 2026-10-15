/* Included once, inside class Phreeqc, directly after the declaration of ineq.
   No include guard, no #include, no access specifier: the declarations below
   keep the access of the surrounding class body (phreeqc_newton_solver). */
	NewtonLinearSolverState *newton_linear_solver_state = nullptr;
	int ineq_cl1_original(int in_kode);
	int newton_solver_ineligibility_reason(int in_kode) const;
	const LDBLE *augmented_newton_matrix(void) const;
	int newton_solver_before_cl1(int in_kode, int k, int l, int m, int n, LDBLE *ineq_array_data,
	                             LDBLE *delta1_data, LDBLE *res_data, int *l_kode, int *l_iter, LDBLE *l_error);
	void newton_solver_after_cl1(int l_kode, LDBLE *delta1_data);
	void newton_solver_destroy_state(void);
