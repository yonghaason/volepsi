#include "PSSender.h"
#include <cryptoTools/Crypto/AES.h>
#include <libOTe/TwoChooseOne/Silent/SilentOtExtReceiver.h>

#include <iterator>

namespace volePSI
{
	void PSSender::init(u64 N, u64 numThreads, std::vector<int> perm)
	{
		this->mN = N;
		this->mLogN = ceil(log2(N));
		this->mNumBenesColumns = 2 * mLogN - 1;
		this->mNumThreads = numThreads;
		mNumSwitches = mNumBenesColumns * (mN / 2);

		benes.initialize(N, perm);
		benes.genBenesRoute();
		setTimePoint("PSSender::initialize");
	}

	Proto PSSender::genOT(oc::PRNG &prng, std::vector<block> &rotRecvMsgs, oc::BitVector &rotChoices, Socket &chl)
	{
		MC_BEGIN(Proto, this, &prng, &rotRecvMsgs, &rotChoices, &chl,
				 otReceiver = std::make_unique<oc::SilentOtExtReceiver>());
		otReceiver->configure(mNumSwitches, 2, mNumThreads);
		MC_AWAIT(otReceiver->genSilentBaseOts(prng, chl));
		rotChoices.resize(mNumSwitches);
		rotRecvMsgs.resize(mNumSwitches);
		MC_AWAIT(otReceiver->silentReceive(rotChoices, rotRecvMsgs, prng, chl));
		MC_END();
	}

	Proto PSSender::genBenes(
		std::vector<std::array<block, 2>> &recvMsg, oc::PRNG &prng, Socket &chl)
	{
		MC_BEGIN(Proto, this, &recvMsg, &prng, &chl,
				 rotRecvMsgs = std::vector<block>{},
				 rotChoices = oc::BitVector{},
				 switches = oc::BitVector{},
				 bitCorrection = oc::BitVector{},
				 recvCorr = std::vector<std::array<block, 2>>{},
				 aes = AES{oc::ZeroBlock},
				 temp = std::array<block, 2>{},
				 i = u64{});

		MC_AWAIT(genOT(prng, rotRecvMsgs, rotChoices, chl));
		
		setTimePoint("PSSender::send-OT");
		
		switches = benes.getSwitchesAsBitVec();
		recvMsg.resize(switches.size());
		for (i = 0; i < recvMsg.size(); i++)
		{
			recvMsg[i] = {rotRecvMsgs[i], aes.ecbEncBlock(rotRecvMsgs[i])};
		}
		bitCorrection = switches ^ rotChoices;
		MC_AWAIT(chl.send(bitCorrection));
		setTimePoint("PSSender::send-bitcorrection");

		recvCorr.resize(switches.size());
		MC_AWAIT(chl.recv(recvCorr));
		setTimePoint("PSSender::recv-correction");

		for (i = 0; i < recvMsg.size(); i++)
		{
			if (switches[i] == 1)
			{
				temp[0] = recvCorr[i][0] ^ recvMsg[i][0];
				temp[1] = recvCorr[i][1] ^ recvMsg[i][1];
				recvMsg[i] = {temp[0], temp[1]};
			}
		}
		MC_END();
	}

	Proto PSSender::runPermAndShare(std::vector<block> &share, oc::PRNG &prng, Socket &chl)
	{
		MC_BEGIN(Proto, this, &share, &prng, &chl,
				 recvMsg = std::vector<std::array<block, 2>>{},
				 matrixRecvMsg = std::vector<std::vector<std::array<block, 2>>>{},
				 ctr = u64{},
				 i = u64{},
				 j = u64{});

		std::cout << "PSSender::genBenes" << std::endl;
		MC_AWAIT(genBenes(recvMsg, prng, chl));

		share.resize(mN);
		MC_AWAIT(chl.recv(share));

		matrixRecvMsg.resize(mNumBenesColumns);
		ctr = 0;
		for (i = 0; i < mNumBenesColumns; ++i)
		{
			matrixRecvMsg[i].resize(mN);
			for (j = 0; j < mN / 2; ++j)
				matrixRecvMsg[i][j] = recvMsg[ctr++];
		}

		benes.benesMaskedEval(share, matrixRecvMsg);
		setTimePoint("PSSender::maskedeval");

		MC_END();
	}

	Proto PSSender::genBenes(
		std::vector<std::array<bool, 2>> &recvMsg, oc::PRNG &prng, Socket &chl)
	{
		MC_BEGIN(Proto, this, &recvMsg, &prng, &chl,
				 rotRecvMsgs = std::vector<block>{},
				 rotChoices = oc::BitVector{},
				 switches = oc::BitVector{},
				 bitCorrection = oc::BitVector{},
				 recvCorr = std::vector<std::array<bool, 2>>{},
				 aes = AES{oc::ZeroBlock},
				 temp = std::array<bool, 2>{},
				 i = u64{});

		MC_AWAIT(genOT(prng, rotRecvMsgs, rotChoices, chl));

		setTimePoint("PSSender::send-OT");

		switches = benes.getSwitchesAsBitVec();
		recvMsg.resize(switches.size());
		
		for (i = 0; i < recvMsg.size(); i++)
		{
			recvMsg[i] = {rotRecvMsgs[i].mData[0] & 1, aes.ecbEncBlock(rotRecvMsgs[i]).mData[0] & 1};
		}
		bitCorrection = switches ^ rotChoices;		
		MC_AWAIT(chl.send(bitCorrection));
		setTimePoint("PSSender::send-bitcorrection");

		recvCorr.resize(switches.size());
		MC_AWAIT(chl.recv(recvCorr));
		setTimePoint("PSSender::recv-correction");

		for (i = 0; i < recvMsg.size(); i++)
		{
			if (switches[i] == 1)
			{
				recvMsg[i] = {recvCorr[i][0] ^ recvMsg[i][0], recvCorr[i][1] ^ recvMsg[i][1]};
			}
		}

		MC_END();
	}

	Proto PSSender::runPermAndShare(oc::BitVector &share, oc::PRNG &prng, Socket &chl)
	{
		MC_BEGIN(Proto, this, &share, &prng, &chl,
				 recvMsg = std::vector<std::array<bool, 2>>{},
				 matrixRecvMsg = std::vector<std::vector<std::array<bool, 2>>>{},
				 ctr = u64{},
				 i = u64{},
				 j = u64{});

		MC_AWAIT(genBenes(recvMsg, prng, chl));

		share.resize(mN);
		MC_AWAIT(chl.recv(share));

		matrixRecvMsg.resize(mNumBenesColumns);
		ctr = 0;
		for (i = 0; i < mNumBenesColumns; ++i)
		{
			matrixRecvMsg[i].resize(mN);
			for (j = 0; j < mN / 2; ++j)
				matrixRecvMsg[i][j] = recvMsg[ctr++];
		}
		benes.benesMaskedEval(share, matrixRecvMsg);
		setTimePoint("PSSender::maskedeval");

		MC_END();
	}
}