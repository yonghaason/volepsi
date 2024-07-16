#include "PS_Tests.h"
#include "volePSI/Defines.h"
#include "volePSI/Cpso.h"

#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Network/Session.h"
#include "cryptoTools/Circuit/BetaLibrary.h"
#include "Common.h"
#include "coproto/Socket/LocalAsyncSock.h"

using namespace volePSI;
using namespace oc;

void Psu_full_test(const oc::CLP &cmd)
{
	CpsuSender sender;
	CpsuReceiver recver;

	auto sockets = cp::LocalAsyncSocket::makePair();

	u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
	u64 nt = cmd.getOr("nt", 1);

	recver.init(n, n, 0, 40, block(0, 0), nt);
	sender.init(n, n, 0, 40, block(0, 1), nt);

	std::random_device rd;
	std::default_random_engine gen(rd());
	std::uniform_int_distribution<u64> dis;
	
	PRNG prng(block(dis(gen), dis(gen)));

	std::vector<block> senderSet(n);
	std::vector<block> receiverSet(n);
	prng.get(senderSet.data(), n);

	receiverSet = senderSet;

	std::vector<block> setDiff;

	auto p0 = sender.send(senderSet, sockets[0]);
	auto p1 = recver.receive(receiverSet, setDiff, sockets[1]);

	eval(p0, p1);

	if (setDiff.size())
		throw RTE_LOC;
}

void Psu_empty_test(const oc::CLP &cmd)
{
	CpsuSender sender;
	CpsuReceiver recver;

	auto sockets = cp::LocalAsyncSocket::makePair();

	u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
	u64 nt = cmd.getOr("nt", 1);

	recver.init(n, n, 0, 40, block(0, 0), nt);
	sender.init(n, n, 0, 40, block(0, 1), nt);
	
	std::random_device rd;
	std::default_random_engine gen(rd());
	std::uniform_int_distribution<u64> dis;
	
	PRNG prng(block(dis(gen), dis(gen)));


	std::vector<block> senderSet(n);
	std::vector<block> receiverSet(n);
	prng.get(senderSet.data(), n);
	prng.get(receiverSet.data(), n);

	std::vector<block> setDiff;

	auto p0 = sender.send(senderSet, sockets[0]);
	auto p1 = recver.receive(receiverSet, setDiff, sockets[1]);

	eval(p0, p1);

	if (setDiff.size() != n)
		throw RTE_LOC;
}

void Psu_partial_test(const oc::CLP &cmd)
{
	std::cout << std::endl;

	CpsuSender sender;
	CpsuReceiver recver;

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

	std::set<block> exp;
	for (u64 i = 0; i < n; ++i)
	{
			if (prng.getBit())
			{
					senderSet[i] = prng.get();
					exp.insert(senderSet[i]);
			}
	}

	std::vector<block> setDiff;

	auto p0 = sender.send(senderSet, sockets[0]);
	auto p1 = recver.receive(receiverSet, setDiff, sockets[1]);
	
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

	std::set<block> setDiffSet(setDiff.begin(), setDiff.end());
	if (setDiffSet != exp) {
		throw RTE_LOC;
	}
}


void Psu_offline_test(const oc::CLP &cmd)
{
	std::cout << std::endl;

	CpsuSender sender;
	CpsuReceiver recver;

	auto sockets = cp::LocalAsyncSocket::makePair();

	u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
	u64 nt = cmd.getOr("nt", 1);

	Timer timer, s, r;

	recver.init(n, n, 0, 40, block(0, 0), nt);
	sender.init(n, n, 0, 40, block(0, 1), nt);

	recver.setTimer(r);
	sender.setTimer(s);
	
	std::random_device rd;
	std::default_random_engine gen(rd());
	std::uniform_int_distribution<u64> dis;
	
	PRNG prng(block(dis(gen), dis(gen)));

	std::vector<block> senderSet(n);
	std::vector<block> receiverSet(n);
	prng.get(senderSet.data(), n);

	receiverSet = senderSet;

	std::set<block> exp;
	for (u64 i = 0; i < n; ++i)
	{
			if (prng.getBit())
			{
					senderSet[i] = prng.get();
					exp.insert(senderSet[i]);
			}
	}

	std::vector<block> setDiff;

	auto p0 = sender.setup(sockets[0]);
	auto p1 = recver.setup(sockets[1]);

	timer.setTimePoint("start");
	r.setTimePoint("start");
	s.setTimePoint("start");

	eval(p0, p1);

	p0 = sender.send(senderSet, sockets[0]);
	p1 = recver.receive(receiverSet, setDiff, sockets[1]);

	eval(p0, p1);

	timer.setTimePoint("end");

	std::cout << "Total Comm: " 
			<< sockets[0].bytesSent() 
			<< " + " << sockets[1].bytesSent() 
			<< " = " << (sockets[0].bytesSent() + sockets[1].bytesSent())
			<< " bytes" << std::endl;

	std::cout << timer << std::endl;
	std::cout <<"Sender Timer\n" << s << "\nReceiver Timer\n" << r << std::endl;

	std::set<block> setDiffSet(setDiff.begin(), setDiff.end());
	if (setDiffSet != exp) {
		throw RTE_LOC;
	}
}