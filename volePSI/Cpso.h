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
            u64 batchSize = 1;
            while (batchSize < numTriples) 
            {
                batchSize <<= 1;
                if (batchSize == (1ull << 25)) break;
            }

            mOtFactory.init(0, numTriples, batchSize, mNumThreads, Mode::Sender, mPrng.get());
            MC_BEGIN(Proto, this, &chl, setup_end_flag = bool{});
            MC_AWAIT(mOtFactory.generateBaseOts(0, mPrng, chl));
            MC_AWAIT(mOtFactory.expand(chl));
            MC_AWAIT(chl.recv(setup_end_flag));
            comm = chl.bytesSent() - comm;
            setTimePoint("PsoReceiver::CPSI Setup");
            // std::cout << "PsoReceiver::CPSI setup = " << (double) comm / (1 << 20) << " MB" << std::endl;
            MC_END();
        }

        Proto receiveCardinality(span<block> X, u64& cardinality, Socket& chl);

        Proto receiveSum(span<block> X, block& sum, Socket& chl);

        // TODO: Template
        Proto receiveSum(span<block> X, int32_t& sum, Socket& chl);

        Proto receiveThreshold(span<block> X, bool& res, const u64 threshold, Socket& chl);

        Proto receiveInnerProd(span<block> X, span<int32_t> data, int32_t& innerprod, Socket& chl);

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
            u64 batchSize = 1;
            while (batchSize < numTriples) 
            {
                batchSize <<= 1;
                if (batchSize == (1ull << 25)) break;
            }
            
            mOtFactory.init(0, numTriples, batchSize, mNumThreads, Mode::Receiver, mPrng.get());

            MC_BEGIN(Proto, this, &chl, setup_end_flag = bool{});
            MC_AWAIT(mOtFactory.generateBaseOts(1, mPrng, chl));
            MC_AWAIT(mOtFactory.expand(chl));
            MC_AWAIT(chl.send(setup_end_flag));
            comm = chl.bytesSent() - comm;
            setTimePoint("PsoSender::CPSI Setup");
            // std::cout << "PsoReceiver::CPSI setup = " << (double) comm / (1 << 20) << " MB" << std::endl;
            MC_END();            
        }

        Proto sendCardinality(span<block> Y, Socket& chl);

        Proto sendSum(span<block> Y, span<block> vals, Socket& chl);

        // TODO: Template
        Proto sendSum(span<block> Y, span<int32_t> vals, Socket& chl);

        Proto sendThreshold(span<block> Y, Socket& chl);

        Proto sendInnerProd(span<block> Y, span<int32_t> data, Socket& chl);
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
            u64 batchSize = 1;
            while (batchSize < numTriples) 
            {
                batchSize <<= 1;
                if (batchSize == (1ull << 25)) break;
            }
            
            mOtFactory.init(numOts, numTriples, batchSize, mNumThreads, Mode::Receiver, mPrng.get());
            MC_BEGIN(Proto, this, &chl, comm = u64{}, setup_end_flag = bool{});
            comm = chl.bytesSent();
            MC_AWAIT(mOtFactory.generateBaseOts(1, mPrng, chl));

            MC_AWAIT(mOtFactory.expandOt(chl));
            // MC_AWAIT(chl.send(setup_end_flag));
            // comm = chl.bytesSent() - comm;
            // setTimePoint("PsuSender::P&S setup");
            // std::cout << "PsuSender::P&S setup = " << (double) comm / (1 << 20) << " MB" << std::endl;

            MC_AWAIT(mOtFactory.expandTriple(chl));
            MC_AWAIT(chl.send(setup_end_flag));
            comm = chl.bytesSent() - comm;
            setTimePoint("PsuSender::CPSI, P&S setup");
            // std::cout << "PsuSender::CPSI setup = " << (double) comm / (1 << 20) << " MB" << std::endl;
            
            mOtFactory.mHasBase = false;
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
            u64 batchSize = 1;
            while (batchSize < (numTriples + numOts)) 
            {
                batchSize <<= 1;
                if (batchSize == (1ull << 25)) break;
            }

            mOtFactory.init(numOts, numTriples, batchSize, mNumThreads, Mode::Sender, mPrng.get());
            MC_BEGIN(Proto, this, &chl, comm = u64{}, setup_end_flag = bool{});
            comm = chl.bytesSent();
            MC_AWAIT(mOtFactory.generateBaseOts(0, mPrng, chl));
            
            MC_AWAIT(mOtFactory.expandOt(chl));
            // MC_AWAIT(chl.recv(setup_end_flag));
            // comm = chl.bytesSent() - comm;
            // setTimePoint("PsuReceiver::P&S setup");
            // std::cout << "PsuReceiver::P&S setup = " << (double) comm / (1 << 20) << " MB" << std::endl;

            MC_AWAIT(mOtFactory.expandTriple(chl));
            MC_AWAIT(chl.recv(setup_end_flag));
            comm = chl.bytesSent() - comm;
            setTimePoint("PsuReceiver::CPSI, P&S setup");
            // std::cout << "PsuReceiver::CPSI setup = " << (double) comm / (1 << 20) << " MB" << std::endl;
            
            mOtFactory.mHasBase = false;
            MC_END();            
        }
        Proto receive(span<block> X, std::vector<block>& D, Socket& chl);
    };
}

#endif