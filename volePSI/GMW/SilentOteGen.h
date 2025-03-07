#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "volePSI/Defines.h"
#include "volePSI/config.h"
#ifdef VOLE_PSI_ENABLE_GMW

#include <vector>
#include <libOTe/TwoChooseOne/Silent/SilentOtExtSender.h>
#include <libOTe/TwoChooseOne/Silent/SilentOtExtReceiver.h>

#include "libOTe/TwoChooseOne/SoftSpokenOT/SoftSpokenShOtExt.h"

namespace volePSI
{
    class SilentOteGen : public oc::TimerAdapter
    {
    public:
        bool mHasBase = false;
        u64 mBatchSize = 0, mNumOts = 0, mNumOtsPer = 0;
        u64 mNumTriples = 0, mNumTriplesPer = 0;
        u64 mNumOteBatches = 0, mNumTripleBatches = 0;
        Mode mMode;
        oc::PRNG mPrng;

        std::unique_ptr<oc::SilentOtExtSender[]> mBackingSenderOT;
        std::unique_ptr<oc::SilentOtExtReceiver[]> mBackingRecverOT;

        span<oc::SilentOtExtSender> mSenderOT;
        span<oc::SilentOtExtReceiver> mRecverOT;

        void init(u64 numOts, u64 numTriples, u64 batchSize, u64 numThreads, Mode mode, block seed);

        void init(u64 n, u64 batchSize, u64 numThreads, Mode mode, block seed) {
            init(0, n, batchSize, numThreads, mode, seed);
        }

        RequiredBase mBase;

        RequiredBase requiredBaseOts();

        void setBaseOts(span<block> recvOts, span<std::array<block, 2>> sendOts);

        Proto expandOt(coproto::Socket& chl);
        Proto expandTriple(coproto::Socket& chl);

        Proto expand(coproto::Socket& chl) {
            MC_BEGIN(Proto, this, &chl, comm = u64{});
            // comm = chl.bytesSent();

            MC_AWAIT(expandOt(chl));
            MC_AWAIT(expandTriple(chl));

            mHasBase = false;
            MC_END();
        }

        bool hasBaseOts()  { return mHasBase; }

        Proto generateBaseOts(u64 partyIdx, oc::PRNG& prng, coproto::Socket& chl)
        {
            MC_BEGIN(Proto,this, partyIdx, &prng, &chl,
                rMsg = oc::AlignedUnVector<block>{},
                sMsg = oc::AlignedUnVector<std::array<block, 2>>{},
                b = RequiredBase{},
                baseOtSender = std::move(oc::SoftSpokenShOtSender{}),
                baseOtRecver = std::move(oc::SoftSpokenShOtReceiver{})
            );

            setTimePoint("TripleGen::generateBaseOts begin");

            b = requiredBaseOts();
            if (b.mNumSend || b.mRecvChoiceBits.size())
            {

                rMsg.resize(b.mRecvChoiceBits.size());
                sMsg.resize(b.mNumSend);
                
                if (partyIdx) { // Receiver
                    MC_AWAIT(baseOtRecver.receive(b.mRecvChoiceBits, rMsg, prng, chl));
                }
                else {
                    MC_AWAIT(baseOtSender.send(sMsg, prng, chl));
                }
                setBaseOts(rMsg, sMsg);
            }
            setTimePoint("TripleGen::generateBaseOts end");

            MC_END();
        }

        // OT extension
        std::vector<std::array<block, 2>> mSendMsgs;
        std::vector<block> mRecvMsgs;
        oc::BitVector mRecvChoices;

        // Triple: A * C = B + D
        span<block> mMult, mAdd;
        std::vector<block> mAVec, mBVec, mDVec;
        oc::BitVector mCBitVec;
    };
}
#endif