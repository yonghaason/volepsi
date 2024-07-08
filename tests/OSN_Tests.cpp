#include "OSN_Tests.h"
#include "volePSI/Defines.h"
#include "volePSI/PS/OSNReceiver.h"
#include "volePSI/PS/OSNSender.h"

#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Network/Session.h"
#include "cryptoTools/Circuit/BetaLibrary.h"
#include "Common.h"
#include "coproto/Socket/LocalAsyncSock.h"

using namespace volePSI;
using namespace oc;

void OText_Test(const oc::CLP &cmd)
{
	OSNReceiver recver;
	OSNSender sender;

	auto sockets = cp::LocalAsyncSocket::makePair();

	u64 n = cmd.getOr("n", 100);
	u64 t = cmd.getOr("t", 1);

	recver.init(n, t);
	sender.init(n, t);
	
	PRNG prng0(block(0, 0));
	PRNG prng1(block(0, 1));

	auto p0 = sender.genOT(prng0, sockets[0]);
	auto p1 = recver.genOT(prng1, sockets[1]);

	eval(p0, p1);

	auto choices = sender.mOtChoices;
	auto recvMsgs = sender.mOtRecvMsgs;
	auto sendMsgs = recver.mSendMsgs;

	for (u64 i = 0; i < n; ++i)
	{
		if (sendMsgs[i][choices[i]] != recvMsgs[i])
			throw RTE_LOC;
	}		
}
