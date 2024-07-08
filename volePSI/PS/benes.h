//#pragma once

#include <vector>

#include <cryptoTools/Common/BitVector.h>

class Benes
{
	int mN;
	int mNumColumns;
	int mLogN;
	
	std::vector<int> perm;
	std::vector<int> invPerm;
	std::vector<std::vector<int>> switched;

	void DFS(int idx, int route, std::vector<char>& path);

public:

	void initialize(int N);

	void genBenesRoute(int colEnd, int colStart, int permIdx, const std::vector<int>& src,
		const std::vector<int>& dest, bool verbose = true);

	void benesEval(int colEnd, int colStart, int permIdx, std::vector<int>& src);

	void benesMaskedEval(int colEnd, int colStart, int permIdx, std::vector<oc::block>& src,
		std::vector<std::vector<std::array<osuCrypto::block, 2>>>& otMsgs);

	osuCrypto::BitVector getSwitches();

};