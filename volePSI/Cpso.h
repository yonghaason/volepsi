#pragma once

#include "volePSI/Defines.h"
#include "volePSI/config.h"
#ifdef VOLE_PSI_ENABLE_CPSI
#include "volePSI/RsCpsi.h"
#include "volePSI/GMW/SilentOteGen.h"

#include "cryptoTools/Crypto/PRNG.h"
#include "cryptoTools/Network/Channel.h"
#include "cryptoTools/Common/Timer.h"
#include "cryptoTools/Circuit/BetaLibrary.h"

namespace volePSI
{
    class PsoReceiver : public details::RsCpsiBase, public oc::TimerAdapter
    {
        SilentOteGen mOtFactory;
        oc::BetaLibrary lib;

    public:
        Proto setup(Socket& chl) {
            auto cuckoo = oc::CuckooIndex<>();
            cuckoo.init(mRecverSize, mSsp, 0, 3);
            auto numBins = cuckoo.mBins.size();
            auto keyBitLength = mSsp + oc::log2ceil(numBins);
            u64 numTriples = 128 * oc::divCeil(numBins, 128) * (keyBitLength - 1) * 2;
            mSetup = true;

            mOtFactory.init(0, numTriples, mOteBatchSize, mNumThreads, Mode::Sender, mPrng.get());
            MC_BEGIN(Proto, this, &chl);
            MC_AWAIT(mOtFactory.generateBaseOts(0, mPrng, chl));
            MC_AWAIT(mOtFactory.expand(chl));
            setTimePoint("Setup::generate ROT & Triples");
            MC_END();
        }

        Proto receiveCardinality(span<block> X, u64& cardinality, Socket& chl);

        Proto receiveSum(span<block> X, block& sum, Socket& chl);

        Proto receiveThreshold(span<block> X, bool& res, const u64 threshold, Socket& chl);

        Proto receiveInnerProd(span<block> X, span<u64> data, u64& innerprod, Socket& chl);

    };

    class PsoSender : public details::RsCpsiBase, public oc::TimerAdapter
    {
        SilentOteGen mOtFactory;
        oc::BetaLibrary lib;

    public:
        Proto setup(Socket& chl) {
            auto cuckoo = oc::CuckooIndex<>();
            cuckoo.init(mRecverSize, mSsp, 0, 3);
            auto numBins = cuckoo.mBins.size();
            auto keyBitLength = mSsp + oc::log2ceil(numBins);
            u64 numTriples = 128 * oc::divCeil(numBins, 128) * (keyBitLength - 1) * 2;
            mSetup = true;
            
            mOtFactory.init(0, numTriples, mOteBatchSize, mNumThreads, Mode::Receiver, mPrng.get());

            MC_BEGIN(Proto, this, &chl);
            MC_AWAIT(mOtFactory.generateBaseOts(1, mPrng, chl));
            MC_AWAIT(mOtFactory.expand(chl));
            setTimePoint("Setup::generate ROTs");
            MC_END();            
        }

        Proto sendCardinality(span<block> Y, Socket& chl);

        Proto sendSum(span<block> Y, span<block> vals, Socket& chl);

        Proto sendThreshold(span<block> Y, Socket& chl);

        Proto sendInnerProd(span<block> Y, span<u64> data, Socket& chl);
    };
    

    class CpsuSender : public details::RsCpsiBase, public oc::TimerAdapter
    {
        SilentOteGen mOtFactory;

    public:
        block dummyBlock = oc::AllOneBlock;
        Proto setup(Socket& chl) {
            auto cuckoo = oc::CuckooIndex<>();
            cuckoo.init(mRecverSize, mSsp, 0, 3);
            auto numBins = cuckoo.mBins.size();
            auto keyBitLength = mSsp + oc::log2ceil(numBins);
            u64 numTriples = 128 * oc::divCeil(numBins, 128) * (keyBitLength - 1) * 2;
            u64 numOts = (2*oc::log2ceil(numBins) - 1) * (numBins / 2);            
            mSetup = true;
            
            mOtFactory.init(numOts, numTriples, mOteBatchSize, mNumThreads, Mode::Receiver, mPrng.get());

            MC_BEGIN(Proto, this, &chl);
            MC_AWAIT(mOtFactory.generateBaseOts(1, mPrng, chl));
            MC_AWAIT(mOtFactory.expand(chl));
            setTimePoint("Setup::generate ROT & Triples");
            MC_END();            
        }
        Proto send(span<block> Y, Socket& chl);
    };

    class CpsuReceiver : public details::RsCpsiBase, public oc::TimerAdapter
    {
        SilentOteGen mOtFactory;

    public:
        block dummyBlock = oc::AllOneBlock;
        Proto setup(Socket& chl) {
            auto cuckoo = oc::CuckooIndex<>();
            cuckoo.init(mRecverSize, mSsp, 0, 3);
            auto numBins = cuckoo.mBins.size();
            auto keyBitLength = mSsp + oc::log2ceil(numBins);
            u64 numTriples = 128 * oc::divCeil(numBins, 128) * (keyBitLength - 1) * 2;
            u64 numOts = (2*oc::log2ceil(numBins) - 1) * (numBins / 2);
            mSetup = true;

            mOtFactory.init(numOts, numTriples, mOteBatchSize, mNumThreads, Mode::Sender, mPrng.get());
            MC_BEGIN(Proto, this, &chl);
            MC_AWAIT(mOtFactory.generateBaseOts(0, mPrng, chl));
            MC_AWAIT(mOtFactory.expand(chl));
            setTimePoint("Setup::generate ROT & Triples");
            MC_END();
        }
        Proto receive(span<block> X, std::vector<block>& D, Socket& chl);
    };
}

#endif