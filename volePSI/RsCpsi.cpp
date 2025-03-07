#include "RsCpsi.h"

#include <sstream>
#include <map>

namespace volePSI
{
    Proto RsCpsiSender::send(span<block> Y, oc::MatrixView<u8> values, Sharing &ret, Socket &chl)
    {
        MC_BEGIN(Proto, this, Y, values, &ret, &chl,
                 cuckooSeed = block{},
                 params = oc::CuckooParam{},
                 numBins = u64{},
                 sIdx = SimpleIndex{},
                 keyBitLength = u64{},
                 keyByteLength = u64{},
                 hashers = std::array<oc::AES, 3>{},
                 Ty = std::vector<block>{},
                 Tv = Matrix<u8>{},
                 r = Matrix<u8>{},
                 TyIter = std::vector<block>::iterator{},
                 TvIter = Matrix<u8>::iterator{},
                 rIter = Matrix<u8>::iterator{},
                 opprf = std::make_unique<RsOpprfSender>(),
                 cir = BetaCircuit{}
                );

        if (mSenderSize != Y.size() || mValueByteLength != values.cols())
            throw RTE_LOC;

        MC_AWAIT(chl.recv(cuckooSeed));
        
        params = oc::CuckooIndex<>::selectParams(mRecverSize, mSsp, 0, 3);
        numBins = params.numBins();
        sIdx.init(numBins, mSenderSize, mSsp, 3);
        sIdx.insertItems(Y, cuckooSeed);

        keyBitLength = mSsp + oc::log2ceil(params.numBins());
        keyByteLength = oc::divCeil(keyBitLength, 8);

        hashers[0].setKey(block(3242, 23423) ^ cuckooSeed);
        hashers[1].setKey(block(4534, 45654) ^ cuckooSeed);
        hashers[2].setKey(block(5677, 67867) ^ cuckooSeed);

        // The OPPRF input value of the i'th input under the j'th cuckoo
        // hash function.
        Ty.resize(Y.size() * 3);

        // The value associated with the k'th OPPRF input
        Tv.resize(Y.size() * 3, keyByteLength + values.cols(), oc::AllocType::Uninitialized);

        // The special value assigned to the i'th bin.
        r.resize(numBins, keyByteLength, oc::AllocType::Uninitialized);

        TyIter = Ty.begin();
        TvIter = Tv.begin();
        rIter = r.begin();
        ret.mValues.resize(numBins, values.cols(), oc::AllocType::Uninitialized);
        mPrng.get<u8>(r);
        mPrng.get<u8>(ret.mValues);

        for (u64 i = 0; i < numBins; ++i)
        {
            auto bin = sIdx.mBins[i];
            auto size = sIdx.mBinSizes[i];

            for (u64 p = 0; p < size; ++p)
            {
                auto j = bin[p].hashIdx();
                auto &hj = hashers[j];
                auto b = bin[p].idx();
                *TyIter = hj.hashBlock(Y[b]);
                memcpy(&*TvIter, &*rIter, keyByteLength);
                TvIter += keyByteLength;

                if (values.size())
                {
                    memcpy(&*TvIter, &values(b, 0), values.cols());

                    if (mType == ValueShareType::Xor)
                    {
                        for (u64 k = 0; k < values.cols(); ++k)
                        {
                            TvIter[k] ^= ret.mValues(i, k);
                        }
                    }
                    else if (mType == ValueShareType::add32)
                    {
                        // assert(values.cols() % sizeof(u32) == 0);
                        auto ss = values.cols() / sizeof(u32);
                        auto tv = (u32 *)TvIter;
                        auto rr = (u32 *)&ret.mValues(i, 0);
                        for (u64 k = 0; k < ss; ++k)
                            tv[k] -= rr[k];
                    }
                    else
                        throw RTE_LOC;
                    TvIter += values.cols();
                }

                ++TyIter;
            }
            rIter += keyByteLength;
        }

        // Why do we need this loop?
        while (TyIter != Ty.end())
        {
            *TyIter = mPrng.get();
            ++TyIter;
        }

        if (mTimer)
            opprf->setTimer(*mTimer);

        MC_AWAIT(opprf->send(mRecverSize, Ty, Tv, mPrng, mNumThreads, chl));

        setTimePoint("RsCpsiSender::OPPRF send");     

        // if (mTimer)
        //     cmp.setTimer(*mTimer);

        cir = isZeroCircuit(keyBitLength);
        cmp.init(r.rows(), cir, mNumThreads, 1, mPrng.get(), mOteBatchSize);

        cmp.setInput(0, r);

        MC_AWAIT(cmp.run(chl));

        {
            auto ss = cmp.getOutputView(0);
            ret.mFlagBits.resize(numBins);
            std::copy(ss.begin(), ss.begin() + ret.mFlagBits.sizeBytes(), ret.mFlagBits.data());
        }
        setTimePoint("RsCpsiSender::GMW/EqShareGen");


        MC_END();
    }

    Proto RsCpsiReceiver::receive(span<block> X, Sharing &ret, Socket &chl)
    {
        MC_BEGIN(Proto, this, X, &ret, &chl,
                 cuckooSeed = block{},
                 cuckoo = oc::CuckooIndex<>{},
                 Tx = std::vector<block>{},
                 hashers = std::array<oc::AES, 3>{},
                 numBins = u64{},
                 keyBitLength = u64{},
                 keyByteLength = u64{},
                 r = Matrix<u8>{},
                 opprf = std::make_unique<RsOpprfReceiver>(),
                 cir = BetaCircuit{},
                 prfTx = Matrix<u8>{}
                );

        if (mRecverSize != X.size())
            throw RTE_LOC;

        cuckooSeed = mPrng.get();
        MC_AWAIT(chl.send(std::move(cuckooSeed)));
        cuckoo.init(mRecverSize, mSsp, 0, 3);

        cuckoo.insert(X, cuckooSeed);
        Tx.resize(mRecverSize);

        hashers[0].setKey(block(3242, 23423) ^ cuckooSeed);
        hashers[1].setKey(block(4534, 45654) ^ cuckooSeed);
        hashers[2].setKey(block(5677, 67867) ^ cuckooSeed);

        ret.mMapping.resize(X.size(), ~u64(0));
        numBins = cuckoo.mBins.size();
        for (u64 i = 0; i < numBins; ++i)
        {
            auto &bin = cuckoo.mBins[i];
            if (!bin.isEmpty())
            {
                auto j = bin.hashIdx();
                auto b = bin.idx();

                // j = oc::CuckooIndex<>::minCollidingHashIdx(i, cuckoo.mHashes[b], 3, numBins);

                auto &hj = hashers[j];
                Tx[b] = hj.hashBlock(X[b]);
                ret.mMapping[b] = i;
            }
        }

        keyBitLength = mSsp + oc::log2ceil(numBins);
        keyByteLength = oc::divCeil(keyBitLength, 8);
    
        prfTx.resize(X.size(), keyByteLength + mValueByteLength, oc::AllocType::Uninitialized);

        if (mTimer)
            opprf->setTimer(*mTimer);

        MC_AWAIT(opprf->receive(mSenderSize * 3, Tx, prfTx, mPrng, mNumThreads, chl));

        setTimePoint("RsCpsiReceiver::OPPRF receive");

        r.resize(numBins, keyByteLength + mValueByteLength, oc::AllocType::Uninitialized);
        mPrng.get<u8>(r);
        for (u64 i = 0; i < mRecverSize; i++) {
            memcpy(&r(ret.mMapping[i], 0), &prfTx(i, 0), keyByteLength + mValueByteLength);
        }
        
        // if (mTimer)
        //     cmp.setTimer(*mTimer);

        cir = isZeroCircuit(keyBitLength);
        cmp.init(r.rows(), cir, mNumThreads, 0, mPrng.get(), mOteBatchSize);

        cmp.implSetInput(0, r, r.cols());

        MC_AWAIT(cmp.run(chl));

        {
            auto ss = cmp.getOutputView(0);

            ret.mFlagBits.resize(numBins);
            std::copy(ss.begin(), ss.begin() + ret.mFlagBits.sizeBytes(), ret.mFlagBits.data());

            if (mValueByteLength)
            {
                ret.mValues.resize(numBins, mValueByteLength);

                for (u64 i = 0; i < numBins; ++i)
                {
                    std::memcpy(&ret.mValues(i, 0), &r(i, keyByteLength), mValueByteLength);
                }
            }
        }

        setTimePoint("RsCpsiReceiver::GMW/EqShareGen");

        MC_END();
    }

}