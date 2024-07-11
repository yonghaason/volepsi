#include "PS_Tests.h"
#include "volePSI/Defines.h"
#include "volePSI/PS/PSReceiver.h"
#include "volePSI/PS/PSSender.h"

#include "cryptoTools/Network/Channel.h"
#include "cryptoTools/Network/Session.h"
#include "cryptoTools/Network/IOService.h"
#include "Common.h"
#include "coproto/Socket/LocalAsyncSock.h"

using namespace volePSI;
using namespace oc;
using namespace coproto;

void OText_test(const oc::CLP &cmd)
{
	PSReceiver recver;
	PSSender sender;

	auto sockets = LocalAsyncSocket::makePair();

	u64 n = cmd.getOr("n", 100);
	u64 nt = cmd.getOr("nt", 1);

	recver.init(n, nt);
	sender.init(n, nt);
	
	PRNG prng0(block(0, 0));
	PRNG prng1(block(0, 1));

	std::vector<std::array<block, 2>> sendMsgs;
	std::vector<block> recvMsgs;
	oc::BitVector choices;

	auto p0 = sender.genOT(prng0, recvMsgs, choices, sockets[0]);
	auto p1 = recver.genOT(prng1, sendMsgs, sockets[1]);

	eval(p0, p1);

	for (u64 i = 0; i < n; ++i)
	{
		if (sendMsgs[i][choices[i]] != recvMsgs[i])
			throw RTE_LOC;
	}		
}

void PS_blk_test(const oc::CLP &cmd)
{
	PSReceiver recver;
	PSSender sender;

	auto sockets = LocalAsyncSocket::makePair();

	u64 n = cmd.getOr("n", 100);
	u64 t = cmd.getOr("t", 1);

	PRNG prng0(block(0, 0));
	PRNG prng1(block(0, 1));

	std::vector<block> input;
	input.resize(n);
	prng0.get(input.data(), input.size());
	
	recver.init(n, t);
	sender.init(n, t);

	auto perm = sender.getPerm();
	
	std::vector<block> senderShare;
	std::vector<block> receiverShare;

	auto p0 = sender.runPermAndShare(senderShare, prng0, sockets[0]);
	auto p1 = recver.runPermAndShare(input, receiverShare, prng1, sockets[1]);

	eval(p0, p1);

	// std::cout << "act / exp" << std::endl;
	for (u64 i = 0; i < n; ++i)
	{
		// if (i < 10) std::cout << (senderShare[i] ^ receiverShare[i]) << " / " << input[perm[i]] << std::endl;
		if (neq(senderShare[i] ^ receiverShare[i], input[perm[i]]))
			throw RTE_LOC;
	}		
}

void PS_bit_test(const oc::CLP &cmd)
{
	PSReceiver recver;
	PSSender sender;

	auto sockets = LocalAsyncSocket::makePair();

	u64 n = cmd.getOr("n", 100);
	u64 t = cmd.getOr("t", 1);

	PRNG prng0(block(0, 0));
	PRNG prng1(block(0, 1));

	oc::BitVector input;
	input.resize(n);
	input.randomize(prng0);
	
	recver.init(n, t);
	sender.init(n, t);

	auto perm = sender.getPerm();
	
	oc::BitVector senderShare;
	oc::BitVector receiverShare;

	auto p0 = sender.runPermAndShare(senderShare, prng0, sockets[0]);
	auto p1 = recver.runPermAndShare(input, receiverShare, prng1, sockets[1]);

	eval(p0, p1);

	// std::cout << "act / exp" << std::endl;
	for (u64 i = 0; i < n; ++i)
	{
		// if (i < 10) std::cout << (senderShare[i] ^ receiverShare[i]) << " / " << input[perm[i]] << std::endl;
		if ((senderShare[i] ^ receiverShare[i]) != input[perm[i]])
			throw RTE_LOC;
	}		
}
