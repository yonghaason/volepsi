#include "Cpso.h"
#include "volePSI/PS/PSReceiver.h"
#include "volePSI/PS/PSSender.h"

#include <sstream>

namespace volePSI
{

    Proto CpsuSender::send(span<block> Y, Socket &chl)
    {
        MC_BEGIN(Proto, this, Y, &chl,
                 s = RsCpsiReceiver::Sharing{},
                 perm = std::vector<int>{},
                 invPerm = std::vector<int>{},
                 idxShareToSet = std::vector<u64>{},
                 psShare = oc::BitVector{},
                 cpsiReceiver = std::make_unique<RsCpsiReceiver>(),
                 psSender = std::make_unique<PSSender>(),
                 finalOtSender = std::make_unique<oc::SilentOtExtSender>(),
                 finalOtMsgs = std::vector<std::array<block, 2>>{},
                 mask = block{},
                 i = u64{}
                 );

        assert(mSenderSize == Y.size());

        if (mTimer) {
            cpsiReceiver->setTimer(*mTimer);
        }
        cpsiReceiver->init(mSenderSize, mRecverSize, 0, mSsp, mPrng.get(), mNumThreads);
        MC_AWAIT(cpsiReceiver->receive(Y, s, chl));

        if (mTimer) {
            psSender->setTimer(*mTimer);
        }
        psSender->init(s.mFlagBits.size(), mNumThreads);
        MC_AWAIT(psSender->runPermAndShare(psShare, mPrng, chl));

        perm = psSender->getPerm();
        invPerm = psSender->getInvPerm();

        idxShareToSet.resize(perm.size());
        std::fill(idxShareToSet.begin(), idxShareToSet.end(), UINT64_MAX);
        for (i = 0; i < Y.size(); i++)
            idxShareToSet[invPerm[s.mMapping[i]]] = i;

        finalOtMsgs.resize(perm.size());
        for (i = 0; i < perm.size(); i++)
        {
            psShare[i] = psShare[i] ^ s.mFlagBits[perm[i]];
            finalOtMsgs[i][!psShare[i]] = dummyBlock;
            if (idxShareToSet[i] != UINT64_MAX) {
                finalOtMsgs[i][psShare[i]] = Y[idxShareToSet[i]];
            }
            else {
                finalOtMsgs[i][psShare[i]] = dummyBlock;
            }
        }        
        
        if (mTimer) {
            finalOtSender->setTimer(*mTimer);
        }
        finalOtSender->configure(perm.size(), 2, mNumThreads);
        MC_AWAIT(finalOtSender->sendChosen(finalOtMsgs, mPrng, chl));

        MC_END();
    }

    Proto CpsuReceiver::receive(span<block> X, std::vector<block> &D, Socket &chl)
    {
        MC_BEGIN(Proto, this, X, &D, &chl,
                 s = RsCpsiSender::Sharing{},
                 psShare = oc::BitVector{},
                 cpsiSender = std::make_unique<RsCpsiSender>(),
                 psReceiver = std::make_unique<PSReceiver>(),
                 finalOtReceiver = std::make_unique<oc::SilentOtExtReceiver>(),
                 finalOtMsgs = std::vector<block>{},
                 i = u64{}
                 );
        assert(mReceiverSize == X.size());

        if (mTimer) {
            cpsiSender->setTimer(*mTimer);
        }
        cpsiSender->init(mSenderSize, mRecverSize, 0, mSsp, mPrng.get(), mNumThreads);
        MC_AWAIT(cpsiSender->send(X, oc::Matrix<u8>(X.size(), 0), s, chl));

        if (mTimer) {
            psReceiver->setTimer(*mTimer);
        }
        psReceiver->init(s.mFlagBits.size(), mNumThreads);
        MC_AWAIT(psReceiver->runPermAndShare(s.mFlagBits, psShare, mPrng, chl));

        if (mTimer) {
            finalOtReceiver->setTimer(*mTimer);
        }
        finalOtMsgs.resize(psShare.size());
        finalOtReceiver->configure(psShare.size(), 2, mNumThreads);
        MC_AWAIT(finalOtReceiver->receiveChosen(psShare, finalOtMsgs, mPrng, chl));

        for (i = 0; i < finalOtMsgs.size(); i++) {
            if (finalOtMsgs[i] != dummyBlock) {
                D.push_back(finalOtMsgs[i]);
            }
        }

        MC_END();
    }

}