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

void Benes_Test(const oc::CLP& cmd)
{
	u64 n = cmd.getOr("n", 100ull);
	int logn = int(ceil(log2(n)));
	int levels = 2 * logn - 1;

	oc::Timer timer;
	
	std::vector<int> dest;
	dest.resize(n);

	Benes benes;
	benes.setTimer(timer);
	benes.initialize(n, levels);	
	
	std::vector<int> src(n);
	for (int i = 0; i < src.size(); ++i)
		src[i] = dest[i] = i;

	std::random_device rd;
	std::shuffle(dest.begin(), dest.end(), std::default_random_engine(rd()));	

	benes.gen_benes_route(logn, 0, 0, src, dest);

	std::vector<int> test_src = src;
	benes.gen_benes_eval(logn, 0, 0, test_src);

	for (u64 i = 0; i < n; i++)
    {
        if (test_src[i] != dest[i])
        {
            oc::lout << "exp " << dest[i] << std::endl;
            oc::lout << "act " << test_src[i] << std::endl;
            throw RTE_LOC;
        }
    }
}