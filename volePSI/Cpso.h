#pragma once

#include "volePSI/Defines.h"
#include "volePSI/config.h"
#ifdef VOLE_PSI_ENABLE_CPSI
#include "volePSI/RsCpsi.h"
#include "volePSI/GMW/SilentOteGen.h"

#include "cryptoTools/Crypto/PRNG.h"
#include "cryptoTools/Network/Channel.h"
#include "cryptoTools/Common/Timer.h"

namespace volePSI
{
    enum class Operation 
    {
        Cardinality = 1,
        Sum = 2,
        InnerProduct = 3,
        Union = 4,
        PrivateID = 5
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

    class PsoBase : public details::RsCpsiBase, oc::TimerAdapter
    {
    public:        
        void init(
            u64 senderSize,
            u64 recverSize,
            u64 valueByteLength,
            u64 statSecParam,
            block seed,
            u64 numThreads,
            Operation operation,
            u64 oteBatchSize = (1ull << 20),
            ValueShareType type = ValueShareType::Xor);
    };
}

#endif