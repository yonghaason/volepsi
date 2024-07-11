#pragma once

#include "volePSI/Defines.h"
#include "volePSI/Paxos.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Network/Session.h"

#include "benes.h"

namespace volePSI
{
	class PSSender : public oc::TimerAdapter
	{
		size_t mN;
		size_t mLogN;
		size_t mNumBenesColumns;
		size_t mNumSwitches;
		size_t mNumThreads;
		
		Benes benes;
		
		Proto genBenes(std::vector<std::array<block, 2>>& recvMsg, oc:: PRNG& prng, Socket &chl);
		// TODO: Remove some dumb duplications (with a clever use of template?).  
		Proto genBenes(std::vector<std::array<bool, 2>>& recvMsg, oc:: PRNG& prng, Socket &chl);

	public:
		
		Proto genOT(oc:: PRNG& prng, std::vector<block> &rotRecvMsgs, oc::BitVector &rotChoices, Socket &chl);		
		
	// public:
		void init(u64 N, u64 numThreads, std::vector<int> perm = std::vector<int>());
		Proto runPermAndShare(std::vector<block> &share, oc:: PRNG& prng, Socket &chl);
		// TODO: Remove dumb duplications (with a clever use of template?).  
		Proto runPermAndShare(oc::BitVector &share, oc:: PRNG& prng, Socket &chl);

		std::vector<int> getPerm() {return benes.getPerm();};
		std::vector<int> getInvPerm() {return benes.getInvPerm();};
	};
}
