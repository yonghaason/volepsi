#pragma once

#include "volePSI/Defines.h"
#include "volePSI/Paxos.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Network/Session.h"
#include <libOTe/TwoChooseOne/Silent/SilentOtExtReceiver.h>

#include "benes.h"

namespace volePSI
{
	class OSNSender : public oc::TimerAdapter
	{
		size_t mN;
		size_t mLogN;
		size_t mNumBenesColumns;
		size_t mNumOts;
		size_t mNumThreads;

	public:
		oc::SilentOtExtReceiver mOtRecver;
		std::vector<block> mOtRecvMsgs;
		oc::BitVector mOtChoices;

		Benes benes;

		Proto genOT(oc:: PRNG& prng, Socket &chl);		
		Proto genBenes(std::vector<std::array<block, 2>>& recvMsg, oc:: PRNG& prng, Socket &chl);

	// public:
		std::vector<int> dest;

		void init(u64 N, u64 numThreads);
		Proto runOsn(std::vector<block> &share, oc:: PRNG& prng, Socket &chl);
		Proto runOsnBit(oc::BitVector &share, oc:: PRNG& prng, Socket &chl);
	};
}
