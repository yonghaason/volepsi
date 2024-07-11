#include "Benes_Tests.h"
#include "volePSI/Defines.h"
#include "volePSI/PS/benes.h"
#include "Common.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Network/Session.h"
#include "cryptoTools/Common/Log.h"
#include "cryptoTools/Common/Timer.h"
#include <algorithm>

using namespace volePSI;
using namespace std;

using PRNG = oc::PRNG;

void Benes_test(const oc::CLP &cmd)
{
	int n = cmd.getOr("n", 100);

	Benes benes;
	benes.initialize(n);

	benes.genBenesRoute();

	std::vector<int> test(n);
	std::iota(test.begin(), test.end(), 0);
	benes.benesEval(test);

	auto perm = benes.getPerm();

	for (int i = 0; i < n; i++)
	{
		if (test[i] != perm[i])
		{
			oc::lout << "exp " << perm[i] << std::endl;
			oc::lout << "act " << test[i] << std::endl;
			throw RTE_LOC;
		}
	}
}