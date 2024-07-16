#pragma once

#include "volePSI/Defines.h"
#include "volePSI/Paxos.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Network/Session.h"

namespace volePSI
{
	class PSReceiver : public oc::TimerAdapter
	{
		size_t mN;
		size_t mLogN;
		size_t mNumBenesColumns;
		size_t mNumSwitches;
		size_t mNumThreads;
		std::vector<std::array<block, 2>> mRotSendMsgs;

		template <typename ValueType>
		void prepareCorrection(u64 depth, u64 permIdx,
													 std::vector<ValueType> &src,
													 std::vector<std::array<std::array<ValueType, 2>, 2>> &sotMsgs,
													 std::vector<std::array<ValueType, 2>> &corrections);

		// TODO: Remove dumb duplications (with a clever use of template?).  
		void prepareCorrection(u64 depth, u64 permIdx,
													 oc::BitVector &src,
													 std::vector<std::array<std::array<bool, 2>, 2>> &sotMsgs,
													 std::vector<std::array<bool, 2>> &corrections);

		Proto genBenes(std::vector<std::array<block, 2>> &retMasks, oc::PRNG& prng, Socket &chl);
		// TODO: Remove dumb duplications (with a clever use of template?).  
		Proto genBenes(std::vector<oc::BitVector> &retMasks, oc::PRNG& prng, Socket &chl);		

	public:

		Proto genOT(oc::PRNG& prng, Socket &chl);
		void setOT(std::vector<std::array<block, 2>> &rotSendMsgs) {
			assert(rotSendMsgs.size() == mNumSwitches);
			mRotSendMsgs = rotSendMsgs;
		}

		void init(u64 N, u64 numThreads);
		
		Proto runPermAndShare(span<const block> inputs, std::vector<block> &outputs, oc::PRNG& prng, Socket &chl);
		
		// TODO: Remove dumb duplications (with a clever use of template?).  
		Proto runPermAndShare(const oc::BitVector &inputs, oc::BitVector &outputs, oc:: PRNG& prng, Socket &chl);
	};
}
