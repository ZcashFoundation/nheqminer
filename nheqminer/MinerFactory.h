#pragma once

#include <AvailableSolvers.h>

class MinerFactory
{
public:
	MinerFactory(bool use_xenoncat, bool use_cuda_djezo, bool use_silentarmy,
		unsigned int n = 200, unsigned int k = 9)
		: _use_xenoncat(use_xenoncat), _use_cuda_djezo(use_cuda_djezo), _use_silentarmy(use_silentarmy),
		  _n(n), _k(k) {
	}

	~MinerFactory();

	std::vector<ISolver *> GenerateSolvers(int cpu_threads, int cuda_count, int* cuda_en, int* cuda_b, int* cuda_t,
		int opencl_count, int opencl_platf, int* opencl_en, int* opencl_t);
	void ClearAllSolvers();

private:
	std::vector<ISolver *> _solvers;

	bool _use_xenoncat = true;
	bool _use_cuda_djezo = true;
	bool _use_silentarmy = true;

	// Equihash parameters. Anything other than (200, 9) needs the generic solver: the optimised
	// ones are specialised for (200, 9) at compile time.
	unsigned int _n = 200;
	unsigned int _k = 9;

	bool UsesDefaultParameters() const { return _n == 200 && _k == 9; }

	// The parameter sets the generic solver instantiates. Anything else throws inside a worker
	// thread, so it has to be rejected before any solver is created.
	bool SupportedParameters() const;

	ISolver * GenCPUSolver(int use_opt);
	ISolver * GenCUDASolver(int dev_id, int blocks, int threadsperblock);
	ISolver * GenOPENCLSolver(int platf_id, int dev_id);

};

