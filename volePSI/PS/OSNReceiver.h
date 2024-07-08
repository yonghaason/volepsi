#pragma once

#include "volePSI/Defines.h"
#include "volePSI/Paxos.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Network/Session.h"
#include <libOTe/TwoChooseOne/Silent/SilentOtExtSender.h>

namespace volePSI
{
	class OSNReceiver : public oc::TimerAdapter
	{
		size_t mN;
		size_t mLogN;
		size_t mNumBenesColumns;
		size_t mNumOts;
		size_t mNumThreads;

		void prepareCorrection(u64 colEnd, u64 colStart, u64 permIdx,
													 std::vector<block> &src,
													 std::vector<std::array<std::array<block, 2>, 2>> &otMsgs,
													 std::vector<std::array<block, 2>> &correctionBlocks);

	public:
		oc::SilentOtExtSender mOtSender;
		std::vector<std::array<block, 2>> mSendMsgs;

		Proto genOT(oc::PRNG& prng, Socket &chl);
		Proto genBenes(std::vector<std::vector<block>> &retMasks, oc::PRNG& prng, Socket &chl);
		
		void init(u64 N, u64 numThreads);
		Proto runOsn(span<const block> inputs, std::vector<block> &outputs, oc::PRNG& prng, Socket &chl);
		Proto runOsnBit(const oc::BitVector inputs, oc::BitVector &outputs, oc:: PRNG& prng, Socket &chl);
	};
}
