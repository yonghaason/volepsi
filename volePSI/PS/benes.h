//#pragma once

#include <vector>

#include <cryptoTools/Common/BitVector.h>
#include <cryptoTools/Common/Timer.h>

class Benes : public oc::TimerAdapter
{
	oc::Timer* timer;

	std::vector<int> perm;
	std::vector<int> inv_perm;
	std::vector<std::vector<int>> switched;

	void DFS(int idx, int route, std::vector<char>& path);

public:

	bool dump(const std::string& filename);

	bool load(const std::string& filename);

	void initialize(int values, int levels);

	osuCrypto::BitVector return_switches(int N);

	void gen_benes_route(int n, int lvl_p, int perm_idx, const std::vector<int>& src,
		const std::vector<int>& dest, bool verbose = true);

	void gen_benes_eval(int n, int lvl_p, int perm_idx, std::vector<int>& src);

	void gen_benes_masked_evaluate(int n, int lvl_p, int perm_idx, std::vector<oc::block>& src,
		std::vector<std::vector<std::array<osuCrypto::block, 2>>>& ot_output);

	osuCrypto::BitVector return_gen_benes_switches(int values);
};