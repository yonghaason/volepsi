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

void genOT_test(const oc::CLP &cmd)
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

	auto p0 = sender.genOT(prng0, sockets[0]);
	auto p1 = recver.genOT(prng1, sockets[1]);

	eval(p0, p1);

	std::cout << "Total Comm " 
		// << sockets[0].bytesSent() 
		// << " + " << sockets[1].bytesSent() 
		<< " = " << (float) (sockets[0].bytesSent() + sockets[1].bytesSent()) / (1 << 20)
		<< " MB" << std::endl;
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

	for (u64 i = 0; i < n; ++i)
	{
		if (neq(senderShare[i] ^ receiverShare[i], input[perm[i]]))
			throw RTE_LOC;
	}		
}

void PS_bit_test(const oc::CLP &cmd)
{
	PSReceiver recver;
	PSSender sender;

	auto sockets = LocalAsyncSocket::makePair();

	u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
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

	for (u64 i = 0; i < n; ++i)
	{
		if ((senderShare[i] ^ receiverShare[i]) != input[perm[i]])
			throw RTE_LOC;
	}		

	std::cout << "Total Comm " 
			// << sockets[0].bytesSent() 
			// << " + " << sockets[1].bytesSent() 
			<< " = " << (float) (sockets[0].bytesSent() + sockets[1].bytesSent()) / (1 << 20)
			<< " MB" << std::endl;
}
