#include "Pso_Tests.h"
#include "volePSI/Defines.h"
#include "volePSI/Cpso.h"

#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Network/Session.h"
#include "cryptoTools/Circuit/BetaLibrary.h"
#include "Common.h"
#include "coproto/Socket/LocalAsyncSock.h"
#include "coproto/Socket/AsioSocket.h"

using namespace std;
using namespace volePSI;
using namespace oc;

void Pso_card_test(const oc::CLP &cmd)
{
	PsoSender sender;
	PsoReceiver recver;

	Timer timer, s, r;

	auto sockets = cp::LocalAsyncSocket::makePair();

	u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
	u64 nt = cmd.getOr("nt", 1);

	recver.init(n, n, 0, 40, block(0, 0), nt);
	sender.init(n, n, 0, 40, block(0, 1), nt);
	
	std::random_device rd;
	std::default_random_engine gen(rd());
	std::uniform_int_distribution<u64> dis;

	recver.setTimer(r);
	sender.setTimer(s);
	
	PRNG prng(block(dis(gen), dis(gen)));

	std::vector<block> senderSet(n);
	std::vector<block> receiverSet(n);
	prng.get(senderSet.data(), n);

	receiverSet = senderSet;

	std::vector<u64> exp;
	for (u64 i = 0; i < n; ++i)
	{
			if (prng.getBit())
			{
					senderSet[i] = prng.get();
			}
			else {
				exp.push_back(i);
			}
	}

	u64 cardinality = 0;

	auto p0 = sender.sendCard(senderSet, sockets[0]);
	auto p1 = recver.receiveCard(receiverSet, cardinality, sockets[1]);

	timer.setTimePoint("start");
	r.setTimePoint("start");
	s.setTimePoint("start");

	eval(p0, p1);

	timer.setTimePoint("end");

	std::cout << "Total Comm: " 
			<< sockets[0].bytesSent() 
			<< " + " << sockets[1].bytesSent() 
			<< " = " << (sockets[0].bytesSent() + sockets[1].bytesSent())
			<< " bytes" << std::endl;

	std::cout << timer << std::endl;
	std::cout <<"Sender Timer\n" << s << "\nReceiver Timer\n" << r << std::endl;

	// std::cout << cardinality << " / " << exp.size() << std::endl;

	if (cardinality != exp.size())
		throw RTE_LOC;
}