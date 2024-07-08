#include "OSNReceiver.h"

namespace volePSI
{
	void OSNReceiver::init(u64 N, u64 numThreads)
	{
		this->mN = N;
		this->mLogN = ceil(log2(N));
		this->mNumBenesColumns = 2 * mLogN - 1;
		this->mNumThreads = numThreads;
		mNumOts = mNumBenesColumns * mN / 2;
	}

	Proto OSNReceiver::genOT(oc:: PRNG& prng, Socket &chl)
	{
		MC_BEGIN(Proto, this, &prng, &chl);
		mOtSender.configure(mNumOts, 2, mNumThreads);
		MC_AWAIT(mOtSender.genSilentBaseOts(prng, chl));
		mSendMsgs.resize(mNumOts);
		MC_AWAIT(mOtSender.silentSend(mSendMsgs, prng, chl));
		MC_END();
	}

	Proto OSNReceiver::genBenes(std::vector<std::vector<block>> &retMasks, oc::PRNG& prng, Socket &chl)
	{
		MC_BEGIN(Proto, this, &retMasks, &prng, &chl,
			masks = std::vector<block>{},
			otMsgs = std::vector<std::array<std::array<block, 2>, 2>>{},
			bitCorrection = oc::BitVector{},
			correctionBlocks = std::vector<std::array<block, 2>>{},
			temp = block{},
			i = u64{},
			aes = AES{oc::ZeroBlock}
		);
			

		masks.resize(mN);
		retMasks.resize(mN);

		for (i = 0; i < mN; i++)
		{ // we sample the input masks randomly
			temp = prng.get<block>();
			masks[i] = temp;
			retMasks[i].push_back(temp);
		}

		otMsgs.resize(mNumOts);
		MC_AWAIT(genOT(prng, chl)); // sample random ot blocks

		bitCorrection.resize(mNumOts);
		MC_AWAIT(chl.recv(bitCorrection));

		for (i = 0; i < mSendMsgs.size(); i++)
		{
			if (bitCorrection[i] == 1)
			{
				temp = mSendMsgs[i][0];
				mSendMsgs[i][0] = mSendMsgs[i][1];
				mSendMsgs[i][1] = temp;
			}
		}

		for (i = 0; i < otMsgs.size(); i++)
		{
			otMsgs[i][0] = {mSendMsgs[i][0], aes.ecbEncBlock(mSendMsgs[i][0])};
			otMsgs[i][1] = {mSendMsgs[i][1], aes.ecbEncBlock(mSendMsgs[i][1])};
		}

		correctionBlocks.resize(mNumOts);
		prepareCorrection(mLogN, 0, 0, masks, otMsgs, correctionBlocks);
		MC_AWAIT(chl.send(correctionBlocks));
		for (i = 0; i < mN; ++i)
		{
			retMasks[i].push_back(masks[i]);
		}		
		MC_END();
	}

	Proto OSNReceiver::runOsn(span<const block> inputs, std::vector<block> &outputs, oc::PRNG& prng, Socket &chl)
	{
		MC_BEGIN(Proto, this, inputs, &outputs, &prng, &chl,
			retMasks = std::vector<std::vector<block>>{},
			benesInputs = std::vector<block>{},
			i = u64{}
		);

		MC_AWAIT(genBenes(retMasks, prng, chl));
		
		for (i = 0; i < mN; ++i)
			retMasks[i][0] = retMasks[i][0] ^ inputs[i];
		for (i = 0; i < mN; ++i)
			benesInputs.push_back(retMasks[i][0]);
		MC_AWAIT(chl.send(benesInputs));
		for (i = 0; i < mN; ++i)
			outputs.push_back(retMasks[i][1]);

		MC_END();
	}

	void OSNReceiver::prepareCorrection(u64 colEnd, u64 colStart, u64 permIdx, std::vector<block> &src,
										std::vector<std::array<std::array<block, 2>, 2>> &otMsgs,
										std::vector<std::array<block, 2>> &correctionBlocks)
	{
		// ot message M0 = m0 ^ w0 || m1 ^ w1
		//  for each switch: top wire m0 w0 - bottom wires m1, w1
		//  M1 = m0 ^ w1 || m1 ^ w0
		int levels = 2 * colEnd - 1;
		
		int subNetSize = src.size();
		
		block m0, m1, w0, w1, M0[2], M1[2], corrMsg[2];
		std::array<oc::block, 2> tempBlock;
		int baseIdx;

		std::vector<block> bottom1;
		std::vector<block> top1;

		if (subNetSize == 2)
		{
			if (colEnd == 1)
			{
				baseIdx = colStart * (mN / 2) + permIdx;
				m0 = src[0];
				m1 = src[1];
				tempBlock = otMsgs[baseIdx][0];
				memcpy(M0, tempBlock.data(), sizeof(M0));
				w0 = M0[0] ^ m0;
				w1 = M0[1] ^ m1;
				tempBlock = otMsgs[baseIdx][1];
				memcpy(M1, tempBlock.data(), sizeof(M1));
				corrMsg[0] = M1[0] ^ m0 ^ w1;
				corrMsg[1] = M1[1] ^ m1 ^ w0;
				correctionBlocks[baseIdx] = {corrMsg[0], corrMsg[1]};
				M1[0] = m0 ^ w1;
				M1[1] = m1 ^ w0;
				otMsgs[baseIdx][1] = {M1[0], M1[1]};
				src[0] = w0;
				src[1] = w1;
			}
			else
			{
				baseIdx = (colStart + 1) * (mN / 2) + permIdx;
				m0 = src[0];
				m1 = src[1];
				tempBlock = otMsgs[baseIdx][0];
				memcpy(M0, tempBlock.data(), sizeof(M0));
				w0 = M0[0] ^ m0;
				w1 = M0[1] ^ m1;
				tempBlock = otMsgs[baseIdx][1];
				memcpy(M1, tempBlock.data(), sizeof(M1));
				corrMsg[0] = M1[0] ^ m0 ^ w1;
				corrMsg[1] = M1[1] ^ m1 ^ w0;
				correctionBlocks[baseIdx] = {corrMsg[0], corrMsg[1]};
				M1[0] = m0 ^ w1;
				M1[1] = m1 ^ w0;
				otMsgs[baseIdx][1] = {M1[0], M1[1]};
				src[0] = w0;
				src[1] = w1;
			}
			return;
		}

		if (subNetSize == 3)
		{
			baseIdx = colStart * (mN / 2) + permIdx;
			m0 = src[0];
			m1 = src[1];
			tempBlock = otMsgs[baseIdx][0];
			memcpy(M0, tempBlock.data(), sizeof(M0));
			w0 = M0[0] ^ m0;
			w1 = M0[1] ^ m1;
			tempBlock = otMsgs[baseIdx][1];
			memcpy(M1, tempBlock.data(), sizeof(M1));
			corrMsg[0] = M1[0] ^ m0 ^ w1;
			corrMsg[1] = M1[1] ^ m1 ^ w0;
			correctionBlocks[baseIdx] = {corrMsg[0], corrMsg[1]};
			M1[0] = m0 ^ w1;
			M1[1] = m1 ^ w0;
			otMsgs[baseIdx][1] = {M1[0], M1[1]};
			src[0] = w0;
			src[1] = w1;

			baseIdx = (colStart + 1) * (mN / 2) + permIdx;
			m0 = src[1];
			m1 = src[2];
			tempBlock = otMsgs[baseIdx][0];
			memcpy(M0, tempBlock.data(), sizeof(M0));
			w0 = M0[0] ^ m0;
			w1 = M0[1] ^ m1;
			tempBlock = otMsgs[baseIdx][1];
			memcpy(M1, tempBlock.data(), sizeof(M1));
			corrMsg[0] = M1[0] ^ m0 ^ w1;
			corrMsg[1] = M1[1] ^ m1 ^ w0;
			correctionBlocks[baseIdx] = {corrMsg[0], corrMsg[1]};
			M1[0] = m0 ^ w1;
			M1[1] = m1 ^ w0;
			otMsgs[baseIdx][1] = {M1[0], M1[1]};
			src[1] = w0;
			src[2] = w1;

			baseIdx = (colStart + 2) * (mN / 2) + permIdx;
			m0 = src[0];
			m1 = src[1];
			tempBlock = otMsgs[baseIdx][0];
			memcpy(M0, tempBlock.data(), sizeof(M0));
			w0 = M0[0] ^ m0;
			w1 = M0[1] ^ m1;
			tempBlock = otMsgs[baseIdx][1];
			memcpy(M1, tempBlock.data(), sizeof(M1));
			corrMsg[0] = M1[0] ^ m0 ^ w1;
			corrMsg[1] = M1[1] ^ m1 ^ w0;
			correctionBlocks[baseIdx] = {corrMsg[0], corrMsg[1]};
			M1[0] = m0 ^ w1;
			M1[1] = m1 ^ w0;
			otMsgs[baseIdx][1] = {M1[0], M1[1]};
			src[0] = w0;
			src[1] = w1;
			return;
		}

		// partea superioara
		for (int i = 0; i < subNetSize - 1; i += 2)
		{
			baseIdx = (colStart) * (mN / 2) + permIdx + i / 2;
			m0 = src[i];
			m1 = src[i ^ 1];
			tempBlock = otMsgs[baseIdx][0];
			memcpy(M0, tempBlock.data(), sizeof(M0));
			w0 = M0[0] ^ m0;
			w1 = M0[1] ^ m1;
			tempBlock = otMsgs[baseIdx][1];
			memcpy(M1, tempBlock.data(), sizeof(M1));
			corrMsg[0] = M1[0] ^ m0 ^ w1;
			corrMsg[1] = M1[1] ^ m1 ^ w0;
			correctionBlocks[baseIdx] = {corrMsg[0], corrMsg[1]};
			M1[0] = m0 ^ w1;
			M1[1] = m1 ^ w0;
			otMsgs[baseIdx][1] = {M1[0], M1[1]};
			src[i] = w0;
			src[i ^ 1] = w1;

			bottom1.push_back(src[i]);
			top1.push_back(src[i ^ 1]);
		}

		if (subNetSize % 2 == 1)
		{
			top1.push_back(src[subNetSize - 1]);
		}

		prepareCorrection(colEnd - 1, colStart + 1, permIdx + subNetSize / 4, top1, otMsgs, correctionBlocks);
		prepareCorrection(colEnd - 1, colStart + 1, permIdx, bottom1, otMsgs, correctionBlocks);

		// partea inferioara
		for (int i = 0; i < subNetSize - 1; i += 2)
		{
			baseIdx = (colStart + levels - 1) * (mN / 2) + permIdx + i / 2;
			m1 = top1[i / 2];
			m0 = bottom1[i / 2];
			tempBlock = otMsgs[baseIdx][0];
			memcpy(M0, tempBlock.data(), sizeof(M0));
			w0 = M0[0] ^ m0;
			w1 = M0[1] ^ m1;
			tempBlock = otMsgs[baseIdx][1];
			memcpy(M1, tempBlock.data(), sizeof(M1));
			corrMsg[0] = M1[0] ^ m0 ^ w1;
			corrMsg[1] = M1[1] ^ m1 ^ w0;
			correctionBlocks[baseIdx] = {corrMsg[0], corrMsg[1]};
			M1[0] = m0 ^ w1;
			M1[1] = m1 ^ w0;
			otMsgs[baseIdx][1] = {M1[0], M1[1]};
			src[i] = w0;
			src[i ^ 1] = w1;
		}

		int idx = int(ceil(subNetSize * 0.5));
		if (subNetSize % 2 == 1)
		{
			src[subNetSize - 1] = top1[idx - 1];
		}
	}
} // namespace volePSI