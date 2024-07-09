#include "PSReceiver.h"

namespace volePSI
{
	void PSReceiver::init(u64 N, u64 numThreads)
	{
		this->mN = N;
		this->mLogN = ceil(log2(N));
		this->mNumBenesColumns = 2 * mLogN - 1;
		this->mNumThreads = numThreads;
		mNumOts = mNumBenesColumns * mN / 2;
	}

	Proto PSReceiver::genOT(oc::PRNG &prng, std::vector<std::array<block, 2>> &rotSendMsgs, Socket &chl)
	{
		MC_BEGIN(Proto, this, &prng, &rotSendMsgs, &chl);
		mOtSender.configure(mNumOts, 2, mNumThreads);
		MC_AWAIT(mOtSender.genSilentBaseOts(prng, chl));
		rotSendMsgs.resize(mNumOts);
		MC_AWAIT(mOtSender.silentSend(rotSendMsgs, prng, chl));
		MC_END();
	}

	Proto PSReceiver::genBenes(std::vector<std::array<block, 2>> &retMasks, oc::PRNG &prng, Socket &chl)
	{
		MC_BEGIN(Proto, this, &retMasks, &prng, &chl,
						 masks = std::vector<block>{},
						 rotSendMsgs = std::vector<std::array<block, 2>>{},
						 sotMsgs = std::vector<std::array<std::array<block, 2>, 2>>{},
						 bitCorrection = oc::BitVector{},
						 corrections = std::vector<std::array<block, 2>>{},
						 temp = block{},
						 i = u64{},
						 aes = AES{oc::ZeroBlock});

		masks.resize(mN);
		retMasks.resize(mN);

		for (i = 0; i < mN; i++)
		{ // we sample the input masks randomly
			temp = prng.get<block>();
			masks[i] = temp;
			retMasks[i][0] = temp;
		}

		sotMsgs.resize(mNumOts);
		MC_AWAIT(genOT(prng, rotSendMsgs, chl)); // sample random ot blocks

		bitCorrection.resize(mNumOts);
		MC_AWAIT(chl.recv(bitCorrection));

		for (i = 0; i < rotSendMsgs.size(); i++)
		{
			if (bitCorrection[i] == 1)
			{
				temp = rotSendMsgs[i][0];
				rotSendMsgs[i][0] = rotSendMsgs[i][1];
				rotSendMsgs[i][1] = temp;
			}
		}

		for (i = 0; i < sotMsgs.size(); i++)
		{
			sotMsgs[i][0] = {rotSendMsgs[i][0], aes.ecbEncBlock(rotSendMsgs[i][0])};
			sotMsgs[i][1] = {rotSendMsgs[i][1], aes.ecbEncBlock(rotSendMsgs[i][1])};
		}

		corrections.resize(mNumOts);
		prepareCorrection(0, 0, masks, sotMsgs, corrections);
		MC_AWAIT(chl.send(corrections));
		for (i = 0; i < mN; ++i)
		{
			retMasks[i][1] = masks[i];
		}
		MC_END();
	}

	Proto PSReceiver::runPermAndShare(span<const block> inputs, std::vector<block> &outputs, oc::PRNG &prng, Socket &chl)
	{
		MC_BEGIN(Proto, this, inputs, &outputs, &prng, &chl,
						 retMasks = std::vector<std::array<block, 2>>{},
						 benesInputs = std::vector<block>{},
						 i = u64{});

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

	template <typename T>
	void PSReceiver::prepareCorrection(
			u64 depth, u64 permIdx, std::vector<T> &src,
			std::vector<std::array<std::array<T, 2>, 2>> &otMsgs,
			std::vector<std::array<T, 2>> &corrections)
	{
		// ot message M0 = m0 ^ w0 || m1 ^ w1
		//  for each switch: top wire m0 w0 - bottom wires m1, w1
		//  M1 = m0 ^ w1 || m1 ^ w0
		int coDepth = mLogN - depth;
		int levels = 2 * coDepth - 1;

		int subNetSize = src.size();

		T m0, m1, w0, w1, M0[2], M1[2], corrMsg[2];
		std::array<T, 2> temp;
		int baseIdx;

		std::vector<T> bottom1;
		std::vector<T> top1;

		if (subNetSize == 2)
		{
			if (coDepth == 1)
			{
				baseIdx = depth * (mN / 2) + permIdx;
				m0 = src[0];
				m1 = src[1];
				temp = otMsgs[baseIdx][0];
				memcpy(M0, temp.data(), sizeof(M0));
				w0 = M0[0] ^ m0;
				w1 = M0[1] ^ m1;
				temp = otMsgs[baseIdx][1];
				memcpy(M1, temp.data(), sizeof(M1));
				corrMsg[0] = M1[0] ^ m0 ^ w1;
				corrMsg[1] = M1[1] ^ m1 ^ w0;
				corrections[baseIdx] = {corrMsg[0], corrMsg[1]};
				M1[0] = m0 ^ w1;
				M1[1] = m1 ^ w0;
				otMsgs[baseIdx][1] = {M1[0], M1[1]};
				src[0] = w0;
				src[1] = w1;
			}
			else
			{
				baseIdx = (depth + 1) * (mN / 2) + permIdx;
				m0 = src[0];
				m1 = src[1];
				temp = otMsgs[baseIdx][0];
				memcpy(M0, temp.data(), sizeof(M0));
				w0 = M0[0] ^ m0;
				w1 = M0[1] ^ m1;
				temp = otMsgs[baseIdx][1];
				memcpy(M1, temp.data(), sizeof(M1));
				corrMsg[0] = M1[0] ^ m0 ^ w1;
				corrMsg[1] = M1[1] ^ m1 ^ w0;
				corrections[baseIdx] = {corrMsg[0], corrMsg[1]};
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
			baseIdx = depth * (mN / 2) + permIdx;
			m0 = src[0];
			m1 = src[1];
			temp = otMsgs[baseIdx][0];
			memcpy(M0, temp.data(), sizeof(M0));
			w0 = M0[0] ^ m0;
			w1 = M0[1] ^ m1;
			temp = otMsgs[baseIdx][1];
			memcpy(M1, temp.data(), sizeof(M1));
			corrMsg[0] = M1[0] ^ m0 ^ w1;
			corrMsg[1] = M1[1] ^ m1 ^ w0;
			corrections[baseIdx] = {corrMsg[0], corrMsg[1]};
			M1[0] = m0 ^ w1;
			M1[1] = m1 ^ w0;
			otMsgs[baseIdx][1] = {M1[0], M1[1]};
			src[0] = w0;
			src[1] = w1;

			baseIdx = (depth + 1) * (mN / 2) + permIdx;
			m0 = src[1];
			m1 = src[2];
			temp = otMsgs[baseIdx][0];
			memcpy(M0, temp.data(), sizeof(M0));
			w0 = M0[0] ^ m0;
			w1 = M0[1] ^ m1;
			temp = otMsgs[baseIdx][1];
			memcpy(M1, temp.data(), sizeof(M1));
			corrMsg[0] = M1[0] ^ m0 ^ w1;
			corrMsg[1] = M1[1] ^ m1 ^ w0;
			corrections[baseIdx] = {corrMsg[0], corrMsg[1]};
			M1[0] = m0 ^ w1;
			M1[1] = m1 ^ w0;
			otMsgs[baseIdx][1] = {M1[0], M1[1]};
			src[1] = w0;
			src[2] = w1;

			baseIdx = (depth + 2) * (mN / 2) + permIdx;
			m0 = src[0];
			m1 = src[1];
			temp = otMsgs[baseIdx][0];
			memcpy(M0, temp.data(), sizeof(M0));
			w0 = M0[0] ^ m0;
			w1 = M0[1] ^ m1;
			temp = otMsgs[baseIdx][1];
			memcpy(M1, temp.data(), sizeof(M1));
			corrMsg[0] = M1[0] ^ m0 ^ w1;
			corrMsg[1] = M1[1] ^ m1 ^ w0;
			corrections[baseIdx] = {corrMsg[0], corrMsg[1]};
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
			baseIdx = (depth) * (mN / 2) + permIdx + i / 2;
			m0 = src[i];
			m1 = src[i ^ 1];
			temp = otMsgs[baseIdx][0];
			memcpy(M0, temp.data(), sizeof(M0));
			w0 = M0[0] ^ m0;
			w1 = M0[1] ^ m1;
			temp = otMsgs[baseIdx][1];
			memcpy(M1, temp.data(), sizeof(M1));
			corrMsg[0] = M1[0] ^ m0 ^ w1;
			corrMsg[1] = M1[1] ^ m1 ^ w0;
			corrections[baseIdx] = {corrMsg[0], corrMsg[1]};
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

		prepareCorrection(depth + 1, permIdx + subNetSize / 4, top1, otMsgs, corrections);
		prepareCorrection(depth + 1, permIdx, bottom1, otMsgs, corrections);

		// partea inferioara
		for (int i = 0; i < subNetSize - 1; i += 2)
		{
			baseIdx = (depth + levels - 1) * (mN / 2) + permIdx + i / 2;
			m1 = top1[i / 2];
			m0 = bottom1[i / 2];
			temp = otMsgs[baseIdx][0];
			memcpy(M0, temp.data(), sizeof(M0));
			w0 = M0[0] ^ m0;
			w1 = M0[1] ^ m1;
			temp = otMsgs[baseIdx][1];
			memcpy(M1, temp.data(), sizeof(M1));
			corrMsg[0] = M1[0] ^ m0 ^ w1;
			corrMsg[1] = M1[1] ^ m1 ^ w0;
			corrections[baseIdx] = {corrMsg[0], corrMsg[1]};
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

	
	Proto PSReceiver::genBenes(std::vector<oc::BitVector> &retMasks, oc::PRNG &prng, Socket &chl)
	{
		MC_BEGIN(Proto, this, &retMasks, &prng, &chl,
						 masks = oc::BitVector{},
						 rotSendMsgs = std::vector<std::array<block, 2>>{},
						 sotMsgs = std::vector<std::array<std::array<bool, 2>, 2>>{},
						 bitCorrection = oc::BitVector{},
						 corrections = std::vector<std::array<bool, 2>>{},
						 temp = block{},
						 i = u64{},
						 aes = AES{oc::ZeroBlock});

		masks.resize(mN);
		masks.randomize(prng);

		retMasks.resize(2);
		retMasks[0] = masks;

		sotMsgs.resize(mNumOts);
		MC_AWAIT(genOT(prng, rotSendMsgs, chl)); // sample random ot blocks

		bitCorrection.resize(mNumOts);
		MC_AWAIT(chl.recv(bitCorrection));

		for (i = 0; i < rotSendMsgs.size(); i++)
		{
			if (bitCorrection[i] == 1)
			{
				temp = rotSendMsgs[i][0];
				rotSendMsgs[i][0] = rotSendMsgs[i][1];
				rotSendMsgs[i][1] = temp;
			}
		}

		// seems to be improved more
		for (i = 0; i < sotMsgs.size(); i++)
		{
			sotMsgs[i][0] = {rotSendMsgs[i][0].mData[0] & 1, aes.ecbEncBlock(rotSendMsgs[i][0]).mData[0] & 1};
			sotMsgs[i][1] = {rotSendMsgs[i][1].mData[0] & 1, aes.ecbEncBlock(rotSendMsgs[i][1]).mData[0] & 1};
		}

		corrections.resize(mNumOts);
		prepareCorrection(0, 0, masks, sotMsgs, corrections);		
		MC_AWAIT(chl.send(corrections));
		retMasks[1] = masks;
		MC_END();
	}

	Proto PSReceiver::runPermAndShare(const oc::BitVector &inputs, oc::BitVector &outputs, oc::PRNG &prng, Socket &chl)
	{
		MC_BEGIN(Proto, this, inputs, &outputs, &prng, &chl,
						 retMasks = std::vector<oc::BitVector>{},
						 benesInputs = oc::BitVector{},
						 i = u64{});

		MC_AWAIT(genBenes(retMasks, prng, chl));
		benesInputs = retMasks[0] ^ inputs;
		MC_AWAIT(chl.send(benesInputs));
		outputs = retMasks[1];
		MC_END();
	}

	
	void PSReceiver::prepareCorrection(
			u64 depth, u64 permIdx, oc::BitVector &src,
			std::vector<std::array<std::array<bool, 2>, 2>> &otMsgs,
			std::vector<std::array<bool, 2>> &corrections)
	{
		// ot message M0 = m0 ^ w0 || m1 ^ w1
		//  for each switch: top wire m0 w0 - bottom wires m1, w1
		//  M1 = m0 ^ w1 || m1 ^ w0
		int coDepth = mLogN - depth;
		int levels = 2 * coDepth - 1;

		int subNetSize = src.size();

		bool m0, m1, w0, w1, M0[2], M1[2], corrMsg[2];
		std::array<bool, 2> temp;
		int baseIdx;

		oc::BitVector bottom1;
		oc::BitVector top1;

		if (subNetSize == 2)
		{
			if (coDepth == 1)
			{
				baseIdx = depth * (mN / 2) + permIdx;
				m0 = src[0];
				m1 = src[1];
				temp = otMsgs[baseIdx][0];
				memcpy(M0, temp.data(), sizeof(M0));
				w0 = M0[0] ^ m0;
				w1 = M0[1] ^ m1;
				temp = otMsgs[baseIdx][1];
				memcpy(M1, temp.data(), sizeof(M1));
				corrMsg[0] = M1[0] ^ m0 ^ w1;
				corrMsg[1] = M1[1] ^ m1 ^ w0;
				corrections[baseIdx] = {corrMsg[0], corrMsg[1]};
				M1[0] = m0 ^ w1;
				M1[1] = m1 ^ w0;
				otMsgs[baseIdx][1] = {M1[0], M1[1]};
				src[0] = w0;
				src[1] = w1;
			}
			else
			{
				baseIdx = (depth + 1) * (mN / 2) + permIdx;
				m0 = src[0];
				m1 = src[1];
				temp = otMsgs[baseIdx][0];
				memcpy(M0, temp.data(), sizeof(M0));
				w0 = M0[0] ^ m0;
				w1 = M0[1] ^ m1;
				temp = otMsgs[baseIdx][1];
				memcpy(M1, temp.data(), sizeof(M1));
				corrMsg[0] = M1[0] ^ m0 ^ w1;
				corrMsg[1] = M1[1] ^ m1 ^ w0;
				corrections[baseIdx] = {corrMsg[0], corrMsg[1]};
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
			baseIdx = depth * (mN / 2) + permIdx;
			m0 = src[0];
			m1 = src[1];
			temp = otMsgs[baseIdx][0];
			memcpy(M0, temp.data(), sizeof(M0));
			w0 = M0[0] ^ m0;
			w1 = M0[1] ^ m1;
			temp = otMsgs[baseIdx][1];
			memcpy(M1, temp.data(), sizeof(M1));
			corrMsg[0] = M1[0] ^ m0 ^ w1;
			corrMsg[1] = M1[1] ^ m1 ^ w0;
			corrections[baseIdx] = {corrMsg[0], corrMsg[1]};
			M1[0] = m0 ^ w1;
			M1[1] = m1 ^ w0;
			otMsgs[baseIdx][1] = {M1[0], M1[1]};
			src[0] = w0;
			src[1] = w1;

			baseIdx = (depth + 1) * (mN / 2) + permIdx;
			m0 = src[1];
			m1 = src[2];
			temp = otMsgs[baseIdx][0];
			memcpy(M0, temp.data(), sizeof(M0));
			w0 = M0[0] ^ m0;
			w1 = M0[1] ^ m1;
			temp = otMsgs[baseIdx][1];
			memcpy(M1, temp.data(), sizeof(M1));
			corrMsg[0] = M1[0] ^ m0 ^ w1;
			corrMsg[1] = M1[1] ^ m1 ^ w0;
			corrections[baseIdx] = {corrMsg[0], corrMsg[1]};
			M1[0] = m0 ^ w1;
			M1[1] = m1 ^ w0;
			otMsgs[baseIdx][1] = {M1[0], M1[1]};
			src[1] = w0;
			src[2] = w1;

			baseIdx = (depth + 2) * (mN / 2) + permIdx;
			m0 = src[0];
			m1 = src[1];
			temp = otMsgs[baseIdx][0];
			memcpy(M0, temp.data(), sizeof(M0));
			w0 = M0[0] ^ m0;
			w1 = M0[1] ^ m1;
			temp = otMsgs[baseIdx][1];
			memcpy(M1, temp.data(), sizeof(M1));
			corrMsg[0] = M1[0] ^ m0 ^ w1;
			corrMsg[1] = M1[1] ^ m1 ^ w0;
			corrections[baseIdx] = {corrMsg[0], corrMsg[1]};
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
			baseIdx = (depth) * (mN / 2) + permIdx + i / 2;
			m0 = src[i];
			m1 = src[i ^ 1];
			temp = otMsgs[baseIdx][0];
			memcpy(M0, temp.data(), sizeof(M0));
			w0 = M0[0] ^ m0;
			w1 = M0[1] ^ m1;
			temp = otMsgs[baseIdx][1];
			memcpy(M1, temp.data(), sizeof(M1));
			corrMsg[0] = M1[0] ^ m0 ^ w1;
			corrMsg[1] = M1[1] ^ m1 ^ w0;
			corrections[baseIdx] = {corrMsg[0], corrMsg[1]};
			M1[0] = m0 ^ w1;
			M1[1] = m1 ^ w0;
			otMsgs[baseIdx][1] = {M1[0], M1[1]};
			src[i] = w0;
			src[i ^ 1] = w1;

			bottom1.pushBack(src[i]);
			top1.pushBack(src[i ^ 1]);
		}

		if (subNetSize % 2 == 1)
		{
			top1.pushBack(src[subNetSize - 1]);
		}

		prepareCorrection(depth + 1, permIdx + subNetSize / 4, top1, otMsgs, corrections);
		prepareCorrection(depth + 1, permIdx, bottom1, otMsgs, corrections);

		// partea inferioara
		for (int i = 0; i < subNetSize - 1; i += 2)
		{
			baseIdx = (depth + levels - 1) * (mN / 2) + permIdx + i / 2;
			m1 = top1[i / 2];
			m0 = bottom1[i / 2];
			temp = otMsgs[baseIdx][0];
			memcpy(M0, temp.data(), sizeof(M0));
			w0 = M0[0] ^ m0;
			w1 = M0[1] ^ m1;
			temp = otMsgs[baseIdx][1];
			memcpy(M1, temp.data(), sizeof(M1));
			corrMsg[0] = M1[0] ^ m0 ^ w1;
			corrMsg[1] = M1[1] ^ m1 ^ w0;
			corrections[baseIdx] = {corrMsg[0], corrMsg[1]};
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