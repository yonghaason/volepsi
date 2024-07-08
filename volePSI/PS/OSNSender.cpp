#include "OSNSender.h"
#include <cryptoTools/Crypto/AES.h>

#include <iterator>

namespace volePSI
{
	void OSNSender::init(u64 N, u64 numThreads)
	{
		this->mN = N;
		this->mLogN = ceil(log2(N));
		this->mNumBenesColumns = 2 * mLogN - 1;
		this->mNumThreads = numThreads;
		mNumOts = mNumBenesColumns * mN / 2;

		dest.resize(N);
		benes.initialize(N);

		std::vector<int> src(N);
		for (size_t i = 0; i < src.size(); ++i)
			src[i] = dest[i] = i;

		benes.genBenesRoute(mLogN, 0, 0, src, dest);
	}

	Proto OSNSender::genOT(oc:: PRNG& prng, Socket &chl)
	{
		MC_BEGIN(Proto, this, &prng, &chl);
		mOtRecver.configure(mNumOts, 2, mNumThreads);
		MC_AWAIT(mOtRecver.genSilentBaseOts(prng, chl));
		mOtChoices.resize(mNumOts);
		mOtRecvMsgs.resize(mNumOts);
		MC_AWAIT(mOtRecver.silentReceive(mOtChoices, mOtRecvMsgs, prng, chl));
		MC_END();
	}

	Proto OSNSender::genBenes(std::vector<std::array<block, 2>> &recvMsg, oc:: PRNG& prng, Socket &chl)
	{
		MC_BEGIN(Proto, this, &recvMsg, &prng, &chl,
						 switches = oc::BitVector{},
						 bitCorrection = oc::BitVector{},
						 recvCorr = std::vector<std::array<block, 2>>{},
						 aes = AES{oc::ZeroBlock},
						 temp = std::array<block, 2>{},
						 i = u64{}
		);

		MC_AWAIT(genOT(prng, chl));

		switches = benes.getSwitches();
		recvMsg.resize(switches.size());
		for (i = 0; i < recvMsg.size(); i++)
		{
			recvMsg[i] = {mOtRecvMsgs[i], aes.ecbEncBlock(mOtRecvMsgs[i])};
		}
		bitCorrection = switches ^ mOtChoices;
		MC_AWAIT(chl.send(bitCorrection));

		recvCorr.resize(switches.size());
		MC_AWAIT(chl.recv(recvCorr));

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

	Proto OSNSender::runOsn(std::vector<block> &share, oc:: PRNG& prng, Socket &chl)
	{
		MC_BEGIN(Proto, this, &share, &prng, &chl,
						 recvMsg = std::vector<std::array<block, 2>>{},
						 matrixRecvMsg = std::vector<std::vector<std::array<block, 2>>>{},
						 ctr = u64{},
						 i = u64{},
						 j = u64{}
		);
		
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

		benes.benesMaskedEval(mLogN, 0, 0, share, matrixRecvMsg);
		MC_END();
	}
}