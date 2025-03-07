#include "Cpso.h"
#include "volePSI/PS/PSReceiver.h"
#include "volePSI/PS/PSSender.h"
#include "volePSI/GMW/Gmw.h"

#include <bitset>

#include <sstream>

namespace volePSI
{
    Proto PsoSender::sendCardinality(span<block> Y, Socket &chl)
    {
        MC_BEGIN(Proto, this, Y, &chl,
                 s = RsCpsiReceiver::Sharing{},
                 cpsiReceiver = std::make_unique<RsCpsiReceiver>(),
                 finalOtSender = std::make_unique<oc::SilentOtExtSender>(),
                 randMsgs = std::vector<std::array<oc::block, 2>>{},
                 finalSendMsgs = oc::Matrix<u8>{},
                 finalSendMsgsIter = oc::Matrix<u8>::iterator{},
                 i = u64{},
                 cardByteLength = u64{},
                 cardShare = u64{},
                 tmp = u64{}
                 );

        assert(mSenderSize == Y.size());

        comm = chl.bytesSent();
        // if (mTimer) {
        //     cpsiReceiver->setTimer(*mTimer);
        // }
        cpsiReceiver->init(mSenderSize, mRecverSize, 0, mSsp, mPrng.get(), mNumThreads);
        if (mSetup) {
            cpsiReceiver->setTriple(mOtFactory.mMult, mOtFactory.mAdd, mOtFactory.mNumTriples);
        }
        MC_AWAIT(cpsiReceiver->receive(Y, s, chl));
        setTimePoint("PsoSend::Card::Cpsi receive");

        // comm = chl.bytesSent() - comm;

        cardByteLength = oc::divCeil(oc::log2ceil(s.mFlagBits.size()), 8);

        randMsgs.resize(s.mFlagBits.size());
        MC_AWAIT(finalOtSender->send(randMsgs, mPrng, chl));
        finalSendMsgs.resize(s.mFlagBits.size()+1, cardByteLength, oc::AllocType::Uninitialized);
        finalSendMsgsIter = finalSendMsgs.begin();
        cardShare = 0;
        for (i = 0; i < s.mFlagBits.size(); i++) 
        {            
            tmp = randMsgs[i][0].mData[0] + randMsgs[i][1].mData[0] 
                    + (u64) s.mFlagBits[i];
            cardShare += (u64) s.mFlagBits[i] + 2*randMsgs[i][0].mData[0];
            memcpy(&*finalSendMsgsIter, &tmp, cardByteLength);
            finalSendMsgsIter += cardByteLength; 
        }
        memcpy(&*finalSendMsgsIter, &cardShare, cardByteLength);
        MC_AWAIT(chl.send(std::move(finalSendMsgs)));
        
        setTimePoint("PsoSend::Card::OT send");
        // comm = chl.bytesSent() - comm;
        // commexp = (u64) cardByteLength * s.mFlagBits.size();
        // std::cout << "PsoSender::OT = " << comm << " bytes / expected: " << commexp << " bytes" << std::endl;

        MC_END();
    }

    Proto PsoReceiver::receiveCardinality(span<block> X, u64& cardinality, Socket &chl)
    {
        MC_BEGIN(Proto, this, X, &cardinality, &chl,
                 s = RsCpsiSender::Sharing{},
                 cpsiSender = std::make_unique<RsCpsiSender>(),
                 finalOtReceiver = std::make_unique<oc::SilentOtExtReceiver>(),
                 randMsgs = std::vector<oc::block>{},
                 finalRecvMsgs = oc::Matrix<u8>{},
                 finalRecvMsgsIter = oc::Matrix<u8>::iterator{},
                 randChoice = oc::BitVector{},
                 i = u64{},
                 cardByteLength = u64{},
                 tmp = u64{},
                 cardShare = u64{}
                );
        assert(mReceiverSize == X.size());

        comm = chl.bytesSent();
        // if (mTimer) {
        //     cpsiSender->setTimer(*mTimer);
        // }
        cpsiSender->init(mSenderSize, mRecverSize, 0, mSsp, mPrng.get(), mNumThreads);
        if (mSetup) {
            cpsiSender->setTriple(mOtFactory.mMult, mOtFactory.mAdd, mOtFactory.mNumTriples);
        }
        MC_AWAIT(cpsiSender->send(X, oc::Matrix<u8>(X.size(), 0), s, chl));
        setTimePoint("PsoReceiver::Card::Cpsi send");

        comm = chl.bytesSent() - comm;

        cardByteLength = oc::divCeil(oc::log2ceil(s.mFlagBits.size()), 8);

        randMsgs.resize(s.mFlagBits.size());
        randChoice.resize(s.mFlagBits.size());
        randChoice.randomize(mPrng);
        MC_AWAIT(finalOtReceiver->receive(s.mFlagBits, randMsgs, mPrng, chl));
        finalRecvMsgs.resize(s.mFlagBits.size()+1, cardByteLength, oc::AllocType::Uninitialized);
        MC_AWAIT(chl.recv(finalRecvMsgs));

        finalRecvMsgsIter = finalRecvMsgs.begin();
        cardShare = 0;
        tmp = 0;
        for (i = 0; i < s.mFlagBits.size(); i++) 
        {
            if (!s.mFlagBits[i]) {
                cardShare += (u64) s.mFlagBits[i] - 2*randMsgs[i].mData[0];
            }
            else {
                memcpy(&tmp, &*finalRecvMsgsIter, cardByteLength);
                cardShare += (u64) s.mFlagBits[i] - 2*(tmp - randMsgs[i].mData[0]);                
            }            
            finalRecvMsgsIter += cardByteLength;
        }
        memcpy(&tmp, &*finalRecvMsgsIter, cardByteLength);
        cardinality = (cardShare + tmp) % (1 << (cardByteLength*8));
    
        setTimePoint("PsoReceiver::Card::OT receive");
        // comm = chl.bytesSent() - comm;
        // commexp = 0;
        // std::cout << "PsoReceiver::OT = " << comm << " bytes / expected: " << commexp << " bytes" << std::endl;

        MC_END();
    }

    Proto PsoSender::sendSum(span<block> Y, span<block> vals, Socket &chl)
    {
        MC_BEGIN(Proto, this, Y, vals, &chl,
                 s = RsCpsiReceiver::Sharing{},
                 cpsiReceiver = std::make_unique<RsCpsiReceiver>(),
                 finalOtSender = std::make_unique<oc::SilentOtExtSender>(),
                 finalOtMsgs = std::vector<std::array<block, 2>>{},
                 invMapping = std::vector<u64>{},
                 i = u64{},
                 sum = oc::block{},
                 r = oc::block{}
                 );

        assert(mSenderSize == Y.size());

        comm = chl.bytesSent();
        // if (mTimer) {
        //     cpsiReceiver->setTimer(*mTimer);
        // }
        cpsiReceiver->init(mSenderSize, mRecverSize, 0, mSsp, mPrng.get(), mNumThreads);
        if (mSetup) {
            cpsiReceiver->setTriple(mOtFactory.mMult, mOtFactory.mAdd, mOtFactory.mNumTriples);
        }
        MC_AWAIT(cpsiReceiver->receive(Y, s, chl));
        setTimePoint("PsoSend::Sum::Cpsi receive");

        invMapping.resize(s.mFlagBits.size(), ~u64(0));
        for (i = 0; i < Y.size(); i++) {
            invMapping[s.mMapping[i]] = i;
        }

        finalOtMsgs.resize(s.mFlagBits.size());
        sum = oc::ZeroBlock;
        for (i = 1; i < s.mFlagBits.size(); i++)
        {
            r = mPrng.get();
            finalOtMsgs[i][s.mFlagBits[i]] = r;
            finalOtMsgs[i][!s.mFlagBits[i]] = r;
            if (invMapping[i] != ~u64(0)) {
                finalOtMsgs[i][!s.mFlagBits[i]] += vals[invMapping[i]];
            }
            sum += r;
        }
        finalOtMsgs[0][s.mFlagBits[0]] = oc::ZeroBlock - sum;
        finalOtMsgs[0][!s.mFlagBits[0]] = oc::ZeroBlock - sum;
        if (invMapping[0] != ~u64(0)) {
            finalOtMsgs[0][!s.mFlagBits[0]] += vals[invMapping[0]];
        }

        finalOtSender->configure(s.mFlagBits.size(), 2, mNumThreads);
        MC_AWAIT(finalOtSender->sendChosen(finalOtMsgs, mPrng, chl));
        
        setTimePoint("PsoSend::Sum::OT send");
        // comm = chl.bytesSent() - comm;
        // commexp = (u64) sizeof(oc::block) * 2*s.mFlagBits.size();
        // std::cout << "PsoSender::OT = " << comm << " bytes / expected: " << commexp << " bytes" << std::endl;

        MC_END();
    }


    Proto PsoSender::sendSum(span<block> Y, span<int32_t> vals, Socket &chl)
    {
        MC_BEGIN(Proto, this, Y, vals, &chl,
                 s = RsCpsiReceiver::Sharing{},
                 cpsiReceiver = std::make_unique<RsCpsiReceiver>(),
                 finalRotSender = std::make_unique<oc::SilentOtExtSender>(),
                 finalRotMsgs = std::vector<std::array<block, 2>>{},
                 finalOtMsgs = std::vector<std::array<int32_t, 2>>{},
                 invMapping = std::vector<u64>{},
                 i = u64{},
                 randsum = int32_t{},
                 r = int32_t{}
                 );

        assert(mSenderSize == Y.size());

        comm = chl.bytesSent();
        // if (mTimer) {
        //     cpsiReceiver->setTimer(*mTimer);
        // }
        cpsiReceiver->init(mSenderSize, mRecverSize, 0, mSsp, mPrng.get(), mNumThreads);
        if (mSetup) {
            cpsiReceiver->setTriple(mOtFactory.mMult, mOtFactory.mAdd, mOtFactory.mNumTriples);
        }
        MC_AWAIT(cpsiReceiver->receive(Y, s, chl));
        setTimePoint("PsoSend::Sum::Cpsi receive");

        invMapping.resize(s.mFlagBits.size(), ~u64(0));
        for (i = 0; i < Y.size(); i++) {
            invMapping[s.mMapping[i]] = i;
        }

        finalRotMsgs.resize(s.mFlagBits.size());
        finalRotSender->configure(s.mFlagBits.size(), 2, mNumThreads);
        MC_AWAIT(finalRotSender->send(finalRotMsgs, mPrng, chl));

        finalOtMsgs.resize(s.mFlagBits.size());
        randsum = 0;
        for (i = 1; i < s.mFlagBits.size(); i++)
        {
            r = mPrng.get<int32_t>();
            finalOtMsgs[i][s.mFlagBits[i]] = finalRotMsgs[i][s.mFlagBits[i]].mData[0] + r;
            finalOtMsgs[i][!s.mFlagBits[i]] = finalRotMsgs[i][!s.mFlagBits[i]].mData[0] + r;
            if (invMapping[i] != ~u64(0)) {
                finalOtMsgs[i][!s.mFlagBits[i]] += vals[invMapping[i]];
            }
            randsum += r;
        }
        finalOtMsgs[0][s.mFlagBits[0]] = finalRotMsgs[0][s.mFlagBits[0]].mData[0] - randsum;
        finalOtMsgs[0][!s.mFlagBits[0]] = finalRotMsgs[0][!s.mFlagBits[0]].mData[0] - randsum;
        if (invMapping[0] != ~u64(0)) {
            finalOtMsgs[0][!s.mFlagBits[0]] += vals[invMapping[0]];
        }

        MC_AWAIT(chl.send(finalOtMsgs));
        
        setTimePoint("PsoSend::Sum::OT send");
        // comm = chl.bytesSent() - comm;
        // commexp = (u64) sizeof(oc::block) * 2*s.mFlagBits.size();
        // std::cout << "PsoSender::OT = " << comm << " bytes / expected: " << commexp << " bytes" << std::endl;

        MC_END();
    }

    Proto PsoReceiver::receiveSum(span<block> X, block& sum, Socket &chl)
    {
        MC_BEGIN(Proto, this, X, &sum, &chl,
                 s = RsCpsiSender::Sharing{},
                 cpsiSender = std::make_unique<RsCpsiSender>(),
                 finalOtReceiver = std::make_unique<oc::SilentOtExtReceiver>(),
                 finalOtMsgs = std::vector<oc::block>{},
                 i = u64{}
                );
        assert(mReceiverSize == X.size());

        comm = chl.bytesSent();
        // if (mTimer) {
        //     cpsiSender->setTimer(*mTimer);
        // }
        cpsiSender->init(mSenderSize, mRecverSize, 0, mSsp, mPrng.get(), mNumThreads);
        if (mSetup) {
            cpsiSender->setTriple(mOtFactory.mMult, mOtFactory.mAdd, mOtFactory.mNumTriples);
        }
        MC_AWAIT(cpsiSender->send(X, oc::Matrix<u8>(X.size(), 0), s, chl));
        setTimePoint("PsoReceiver::Sum::Cpsi send");

        finalOtMsgs.resize(s.mFlagBits.size());
        finalOtReceiver->configure(s.mFlagBits.size(), 2, mNumThreads);
        MC_AWAIT(finalOtReceiver->receiveChosen(s.mFlagBits, finalOtMsgs, mPrng, chl));
        sum = oc::ZeroBlock;
        for (i = 0; i < finalOtMsgs.size(); i++) {
            sum += finalOtMsgs[i];
        }
    
        setTimePoint("PsoReceiver::Sum::OT recv");
        // comm = chl.bytesSent() - comm;
        // commexp = 0;
        // std::cout << "PsoReceiver::OT = " << comm << " bytes / expected: " << commexp << " bytes" << std::endl;

        MC_END();
    } // recvSum

    Proto PsoReceiver::receiveSum(span<block> X, int32_t& sum, Socket &chl)
    {
        MC_BEGIN(Proto, this, X, &sum, &chl,
                 s = RsCpsiSender::Sharing{},
                 cpsiSender = std::make_unique<RsCpsiSender>(),
                 finalRotReceiver = std::make_unique<oc::SilentOtExtReceiver>(),
                 finalRotMsgs = std::vector<oc::block>{},
                 finalOtMsgs = std::vector<std::array<int32_t, 2>>{},
                 i = u64{}
                );
        assert(mReceiverSize == X.size());

        comm = chl.bytesSent();
        // if (mTimer) {
        //     cpsiSender->setTimer(*mTimer);
        // }
        cpsiSender->init(mSenderSize, mRecverSize, 0, mSsp, mPrng.get(), mNumThreads);
        if (mSetup) {
            cpsiSender->setTriple(mOtFactory.mMult, mOtFactory.mAdd, mOtFactory.mNumTriples);
        }
        MC_AWAIT(cpsiSender->send(X, oc::Matrix<u8>(X.size(), 0), s, chl));
        setTimePoint("PsoReceiver::Sum::Cpsi send");

        finalRotMsgs.resize(s.mFlagBits.size());
        finalRotReceiver->configure(s.mFlagBits.size(), 2, mNumThreads);
        MC_AWAIT(finalRotReceiver->receive(s.mFlagBits, finalRotMsgs, mPrng, chl));

        finalOtMsgs.resize(s.mFlagBits.size());
        MC_AWAIT(chl.recv(finalOtMsgs));

        sum = 0;
        for (i = 0; i < finalOtMsgs.size(); i++) {
            sum += finalOtMsgs[i][s.mFlagBits[i]] - finalRotMsgs[i].mData[0];
        }
    
        setTimePoint("PsoReceiver::Sum::OT recv");
        // comm = chl.bytesSent() - comm;
        // commexp = 0;
        // std::cout << "PsoReceiver::OT = " << comm << " bytes / expected: " << commexp << " bytes" << std::endl;

        MC_END();
    } // recvSum

    Proto PsoSender::sendThreshold(span<block> Y, Socket &chl)
    {
        MC_BEGIN(Proto, this, Y, &chl,
                 s = RsCpsiReceiver::Sharing{},
                 cpsiReceiver = std::make_unique<RsCpsiReceiver>(),
                 finalOtSender = std::make_unique<oc::SilentOtExtSender>(),
                 randMsgs = std::vector<std::array<oc::block, 2>>{},
                 finalSendMsgs = oc::Matrix<u8>{},
                 finalSendMsgsIter = oc::Matrix<u8>::iterator{},
                 i = u64{},
                 cardByteLength = u64{},
                 cardBitLength = u64{},
                 cardShare = u64{},
                 tmp = u64{},
                 gmw = std::make_unique<volePSI::Gmw>(),
                 addCir = std::make_unique<oc::BetaCircuit>(),
                 gtrCir = std::make_unique<oc::BetaCircuit>(),
                 inCard = oc::Matrix<u8>{},
                 sAdd = oc::Matrix<u8>{},
                 sThr = oc::Matrix<u8>{}
                 );

        assert(mSenderSize == Y.size());
        
        comm = chl.bytesSent();
        // if (mTimer) {
        //     cpsiReceiver->setTimer(*mTimer);
        // }
        cpsiReceiver->init(mSenderSize, mRecverSize, 0, mSsp, mPrng.get(), mNumThreads);
        if (mSetup) {
            cpsiReceiver->setTriple(mOtFactory.mMult, mOtFactory.mAdd, mOtFactory.mNumTriples);
        }
        MC_AWAIT(cpsiReceiver->receive(Y, s, chl));
        setTimePoint("PsoSender::Threshold::Cpsi receive");
        
        cardBitLength = oc::log2ceil(s.mFlagBits.size());
        cardByteLength = oc::divCeil(cardBitLength, 8);

        randMsgs.resize(s.mFlagBits.size());
        MC_AWAIT(finalOtSender->send(randMsgs, mPrng, chl));
        
        finalSendMsgs.resize(s.mFlagBits.size(), cardByteLength, oc::AllocType::Uninitialized);
        finalSendMsgsIter = finalSendMsgs.begin();
        cardShare = 0;
        for (i = 0; i < s.mFlagBits.size(); i++) 
        {            
            tmp = randMsgs[i][0].mData[0] + randMsgs[i][1].mData[0] 
                    + (u64) s.mFlagBits[i];
            cardShare += (u64) s.mFlagBits[i] + 2*randMsgs[i][0].mData[0];
            memcpy(&*finalSendMsgsIter, &tmp, cardByteLength);
            finalSendMsgsIter += cardByteLength; 
        }
        MC_AWAIT(chl.send(std::move(finalSendMsgs)));
        setTimePoint("PsoSender::Threshold::OT send");

        ///////////////////////////
        inCard.resize(1, cardByteLength);
        memcpy(inCard[0].data(), &cardShare, cardByteLength);
        sAdd.resize(1, cardByteLength);
        sThr.resize(1, 1);

        gmw->init(1, *lib.uint_uint_add(cardBitLength, cardBitLength, cardBitLength), 1, 1, oc::OneBlock);        
        gmw->setInput(0, inCard);
        gmw->setInput(1, Matrix<u8>(1, cardByteLength));        
        MC_AWAIT(gmw->run(chl));
        gmw->getOutput(0, sAdd);

        gmw->init(1, *lib.int_int_gteq(cardBitLength, cardBitLength), 1, 1, oc::OneBlock);         
        gmw->setInput(0, sAdd);
        gmw->setInput(1, Matrix<u8>(1, cardByteLength));
        MC_AWAIT(gmw->run(chl));
        gmw->getOutput(0, sThr);

        MC_AWAIT(chl.send(std::move(sThr)));
        
        setTimePoint("PsoSender::Threshold::2PC");
        // comm = chl.bytesSent() - comm;
        // commexp = (u64) cardByteLength * s.mFlagBits.size();
        // std::cout << "PsoSender::OT = " << comm << " bytes / expected: " << commexp << " bytes" << std::endl;

        MC_END();
    }

    Proto PsoReceiver::receiveThreshold(span<block> X, bool& res, const u64 threshold, Socket &chl)
    {
        MC_BEGIN(Proto, this, X, &res, threshold, &chl,
                 s = RsCpsiSender::Sharing{},
                 cpsiSender = std::make_unique<RsCpsiSender>(),
                 finalOtReceiver = std::make_unique<oc::SilentOtExtReceiver>(),
                 randMsgs = std::vector<oc::block>{},
                 finalRecvMsgs = oc::Matrix<u8>{},
                 finalRecvMsgsIter = oc::Matrix<u8>::iterator{},
                 randChoice = oc::BitVector{},
                 i = u64{},
                 cardByteLength = u64{},
                 cardBitLength = u64{},
                 cardShare = u64{},
                 tmp = u64{},
                 gmw = std::make_unique<volePSI::Gmw>(),
                 addCir = std::make_unique<oc::BetaCircuit>(),
                 gtrCir = std::make_unique<oc::BetaCircuit>(),
                 inCard = oc::Matrix<u8>{},
                 inThr = oc::Matrix<u8>{},
                 sAdd = oc::Matrix<u8>{},
                 sThr = oc::Matrix<u8>{},
                 sThrTheir = oc::Matrix<u8>{}
                );
        assert(mReceiverSize == X.size());

        comm = chl.bytesSent();
        // if (mTimer) {
        //     cpsiSender->setTimer(*mTimer);
        // }
        cpsiSender->init(mSenderSize, mRecverSize, 0, mSsp, mPrng.get(), mNumThreads);
        if (mSetup) {
            cpsiSender->setTriple(mOtFactory.mMult, mOtFactory.mAdd, mOtFactory.mNumTriples);
        }
        MC_AWAIT(cpsiSender->send(X, oc::Matrix<u8>(X.size(), 0), s, chl));
        setTimePoint("PsoReceiver::Threshold::Cpsi send");

        // comm = chl.bytesSent() - comm;

        cardBitLength = oc::log2ceil(s.mFlagBits.size());
        cardByteLength = oc::divCeil(cardBitLength, 8);

        randMsgs.resize(s.mFlagBits.size());
        randChoice.resize(s.mFlagBits.size());
        randChoice.randomize(mPrng);
        MC_AWAIT(finalOtReceiver->receive(s.mFlagBits, randMsgs, mPrng, chl));
        finalRecvMsgs.resize(s.mFlagBits.size(), cardByteLength, oc::AllocType::Uninitialized);
        MC_AWAIT(chl.recv(finalRecvMsgs));

        finalRecvMsgsIter = finalRecvMsgs.begin();
        cardShare = 0;
        tmp = 0;
        for (i = 0; i < s.mFlagBits.size(); i++) 
        {
            if (!s.mFlagBits[i]) {
                cardShare += (u64) s.mFlagBits[i] - 2*randMsgs[i].mData[0];
            }
            else {
                memcpy(&tmp, &*finalRecvMsgsIter, cardByteLength);
                cardShare += (u64) s.mFlagBits[i] - 2*(tmp - randMsgs[i].mData[0]);                
            }            
            finalRecvMsgsIter += cardByteLength;
        }
        setTimePoint("PsoReceiver::Threshold::OT recv");

        //////////////////        
        inCard.resize(1, cardByteLength);
        memcpy(inCard[0].data(), &cardShare, cardByteLength);
        inThr.resize(1, cardByteLength);
        memcpy(inThr[0].data(), &threshold, cardByteLength);
        sAdd.resize(1, cardByteLength);    
        sThr.resize(1, 1);

        gmw->init(1, *lib.uint_uint_add(cardBitLength, cardBitLength, cardBitLength), 1, 0, oc::ZeroBlock);
        gmw->setInput(0, Matrix<u8>(1, cardByteLength));
        gmw->setInput(1, inCard);        
        MC_AWAIT(gmw->run(chl));
        gmw->getOutput(0, sAdd);

        gmw->init(1, *lib.int_int_gteq(cardBitLength, cardBitLength), 1, 0, oc::ZeroBlock);
        gmw->setInput(0, sAdd);
        gmw->setInput(1, inThr);
        MC_AWAIT(gmw->run(chl));
        gmw->getOutput(0, sThr);

        sThrTheir.resize(1, 1);

        MC_AWAIT(chl.recv(sThrTheir));
        res = (sThr[0][0] ^ sThrTheir[0][0]) & 1;        

        setTimePoint("PsoReceiver::Threshold::2PC");
        // comm = chl.bytesSent() - comm;
        // commexp = 0;
        // std::cout << "PsoReceiver::FinalOT = " << comm << " bytes / expected: " << commexp << " bytes" << std::endl;

        MC_END();
    }

    Proto PsoSender::sendInnerProd(span<block> Y, span<int32_t> data, Socket& chl)
    {
        MC_BEGIN(Proto, this, Y, data, &chl,
                 s = RsCpsiReceiver::Sharing{},
                 cpsiReceiver = std::make_unique<RsCpsiReceiver>(),
                 clearShares = std::vector<int32_t>{},
                 innerProdShare = int32_t{},
                 invMapping = std::vector<u64>{},
                 reordered_data = std::vector<int32_t>{},

                 rot1Sender = std::make_unique<oc::SilentOtExtSender>(),
                 rot1Msgs = std::vector<std::array<block, 2>>{},
                 ot1Msgs = std::vector<std::array<int32_t, 2>>{},

                 rot2Receiver = std::make_unique<oc::SilentOtExtReceiver>(),
                 rot2Msgs = std::vector<oc::block>{},
                 ot2Msgs = std::vector<std::array<int32_t, 2>>{},
                 
                 msRotReceiver = std::make_unique<oc::SilentOtExtReceiver>(),
                 msRotMsgs = std::vector<oc::block>{},
                 msChoices = oc::BitVector{},
                 msOtMsgs = std::vector<int32_t>{},
                 
                 r = int32_t{},
                 tmp = int32_t{},
                 i = u64{},
                 j = u64{}
                 );

        assert(mSenderSize == Y.size());
        
        comm = chl.bytesSent();
        cpsiReceiver->init(mSenderSize, mRecverSize, sizeof(int32_t), mSsp, mPrng.get(), mNumThreads, (1ull << 20), ValueShareType::add32);
        if (mSetup) {
            cpsiReceiver->setTriple(mOtFactory.mMult, mOtFactory.mAdd, mOtFactory.mNumTriples);
        }
        MC_AWAIT(cpsiReceiver->receive(Y, s, chl));
        setTimePoint("PsoSender::InnerProd::Cpsi receive");

        // Clear out 1st OT 
        rot1Msgs.resize(s.mFlagBits.size());
        rot1Sender->configure(s.mFlagBits.size(), 2, mNumThreads);
        MC_AWAIT(rot1Sender->send(rot1Msgs, mPrng, chl));

        ot1Msgs.resize(s.mFlagBits.size());
        clearShares.resize(s.mFlagBits.size());
        for (i = 0; i < s.mFlagBits.size(); i++)
        {
            r = mPrng.get<int32_t>();
            memcpy(&tmp, s.mValues[i].data(), sizeof(int32_t));
            ot1Msgs[i][s.mFlagBits[i]] = rot1Msgs[i][s.mFlagBits[i]].mData[0] + r;
            ot1Msgs[i][!s.mFlagBits[i]] = rot1Msgs[i][!s.mFlagBits[i]].mData[0] + r + tmp;
            clearShares[i] = -r;
        }
        MC_AWAIT(chl.send(ot1Msgs));
        
        // Clear out 2nd OT 
        rot2Msgs.resize(s.mFlagBits.size());
        rot2Receiver->configure(s.mFlagBits.size(), 2, mNumThreads);
        MC_AWAIT(rot2Receiver->receive(s.mFlagBits, rot2Msgs, mPrng, chl));

        ot2Msgs.resize(s.mFlagBits.size());
        MC_AWAIT(chl.recv(ot2Msgs));

        for (i = 0; i < ot2Msgs.size(); i++) {
            clearShares[i] += ot2Msgs[i][s.mFlagBits[i]] - rot2Msgs[i].mData[0];
        }
        setTimePoint("PsoSender::InnerProd::Clear out");

        // MultShare
        invMapping.resize(s.mFlagBits.size(), ~u64(0));
        for (i = 0; i < Y.size(); i++) {
            invMapping[s.mMapping[i]] = i;
        }

        tmp = 0;
        reordered_data.resize(s.mFlagBits.size());
        for (i = 0; i < s.mFlagBits.size(); i++) 
        {   
            if (invMapping[i] != ~u64(0)) {
                reordered_data[i] = (data[invMapping[i]]);
            }
            else {
                reordered_data[i] = tmp;
            }
        }
        msChoices = oc::BitVector((u8*) reordered_data.data(), s.mFlagBits.size() * 32);
        
        msRotMsgs.resize(msChoices.size());
        msRotReceiver->configure(msChoices.size(), 2, mNumThreads);
        MC_AWAIT(msRotReceiver->receive(msChoices, msRotMsgs, mPrng, chl));

        setTimePoint("PsoSender::InnerProd::Multshare-setup");

        msOtMsgs.resize(msChoices.size());
        MC_AWAIT(chl.recv(msOtMsgs));

        innerProdShare = 0;
        for (i = 0; i < s.mFlagBits.size(); i++)
        {   
            if (invMapping[i] != ~u64(0))
                innerProdShare += clearShares[i] * data[invMapping[i]];
            
            for (j = 0; j < 32; j++)
            {
                if (msChoices[32*i + j])
                    innerProdShare += msOtMsgs[32*i + j] - msRotMsgs[32*i + j].mData[0];
                else 
                    innerProdShare += msRotMsgs[32*i + j].mData[0];
            }
        }
        MC_AWAIT(chl.send(innerProdShare));
        setTimePoint("PsoSender::InnerProd::MultShare");

        // comm = chl.bytesSent() - comm;
        // commexp = (u64) cardByteLength * s.mFlagBits.size();
        // std::cout << "PsoSender::OT = " << comm << " bytes / expected: " << commexp << " bytes" << std::endl;

        MC_END();
    }

    Proto PsoReceiver::receiveInnerProd(span<block> X, span<int32_t> data, int32_t& innerProd, Socket& chl)
    {
        MC_BEGIN(Proto, this, X, data, &innerProd, &chl,
                 s = RsCpsiSender::Sharing{},
                 cpsiSender = std::make_unique<RsCpsiSender>(),
                 dataInMatrix = oc::Matrix<u8>{},
                 clearShares = std::vector<int32_t>{},
                 innerProdShare = int32_t{},
                 innerProdShareTheir = int32_t{},

                 rot1Receiver = std::make_unique<oc::SilentOtExtReceiver>(),
                 rot1Msgs = std::vector<oc::block>{},
                 ot1Msgs = std::vector<std::array<int32_t, 2>>{},

                 rot2Sender = std::make_unique<oc::SilentOtExtSender>(),
                 rot2Msgs = std::vector<std::array<block, 2>>{},
                 ot2Msgs = std::vector<std::array<int32_t, 2>>{},

                 msRotSender = std::make_unique<oc::SilentOtExtSender>(),
                 msRotMsgs = std::vector<std::array<block, 2>>{},
                 msOtMsgs = std::vector<int32_t>{},
                 
                 r = int32_t{},
                 tmp = int32_t{},
                 i = u64{},
                 j = u64{}
                );
        assert(mReceiverSize == X.size());

        comm = chl.bytesSent();
        // if (mTimer) {
        //     cpsiSender->setTimer(*mTimer);
        // }
        dataInMatrix.resize(X.size(), sizeof(int32_t));
        std::memcpy(dataInMatrix.data(), (u8*) data.data(), X.size() * sizeof(int32_t));

        cpsiSender->init(mSenderSize, mRecverSize, sizeof(int32_t), mSsp, mPrng.get(), mNumThreads, (1ull << 20), ValueShareType::add32);
        if (mSetup) {
            cpsiSender->setTriple(mOtFactory.mMult, mOtFactory.mAdd, mOtFactory.mNumTriples);
        }
        MC_AWAIT(cpsiSender->send(X, dataInMatrix, s, chl));
        setTimePoint("PsoReceiver::InnerProd::Cpsi send");

        // Clear out 1st OT 
        rot1Msgs.resize(s.mFlagBits.size());
        rot1Receiver->configure(s.mFlagBits.size(), 2, mNumThreads);
        MC_AWAIT(rot1Receiver->receive(s.mFlagBits, rot1Msgs, mPrng, chl));

        clearShares.resize(s.mFlagBits.size());
        ot1Msgs.resize(s.mFlagBits.size());
        MC_AWAIT(chl.recv(ot1Msgs));

        for (i = 0; i < ot1Msgs.size(); i++) {
            clearShares[i] = ot1Msgs[i][s.mFlagBits[i]] - rot1Msgs[i].mData[0];
        }

        // Clear out 2nd OT 
        rot2Msgs.resize(s.mFlagBits.size());
        rot2Sender->configure(s.mFlagBits.size(), 2, mNumThreads);
        MC_AWAIT(rot2Sender->send(rot2Msgs, mPrng, chl));

        ot2Msgs.resize(s.mFlagBits.size());

        for (i = 0; i < s.mFlagBits.size(); i++)
        {
            r = mPrng.get<int32_t>();
            memcpy(&tmp, s.mValues[i].data(), sizeof(int32_t));
            ot2Msgs[i][s.mFlagBits[i]] = rot2Msgs[i][s.mFlagBits[i]].mData[0] + r;
            ot2Msgs[i][!s.mFlagBits[i]] = rot2Msgs[i][!s.mFlagBits[i]].mData[0] + r + tmp;
            clearShares[i] -= r;
        }

        MC_AWAIT(chl.send(ot2Msgs));

        setTimePoint("PsoReceiver::InnerProd::Clear out");

        // MultShare
        msRotMsgs.resize(s.mFlagBits.size() * 32);
        msRotSender->configure(s.mFlagBits.size() * 32, 2, mNumThreads);
        MC_AWAIT(msRotSender->send(msRotMsgs, mPrng, chl));

        setTimePoint("PsoReceiver::InnerProd::Multshare-setup");
        
        innerProdShare = 0;
        msOtMsgs.resize(s.mFlagBits.size() * 32);
        for (i = 0; i < s.mFlagBits.size(); i++)
        {
            for (j = 0; j < 32; j++)
            {
                msOtMsgs[32*i + j] = msRotMsgs[32*i + j][0].mData[0] + msRotMsgs[32*i + j][1].mData[0] + (clearShares[i] << j);
                innerProdShare -= msRotMsgs[32*i + j][0].mData[0];
            }
        }
        MC_AWAIT(chl.send(msOtMsgs));

        MC_AWAIT(chl.recv(innerProdShareTheir));
        innerProd = innerProdShare + innerProdShareTheir;
        setTimePoint("PsoReceiver::InnerProd::MultShare");

        // comm = chl.bytesSent() - comm;
        // commexp = 0;
        // std::cout << "PsoReceiver::FinalOT = " << comm << " bytes / expected: " << commexp << " bytes" << std::endl;

        MC_END();
    }

    Proto CpsuReceiver::receive(span<block> X, std::vector<block> &D, Socket &chl)
    {
        MC_BEGIN(Proto, this, X, &D, &chl,
                 oprfX = std::vector<block>{},
                 comm = u64{},
                 s = RsCpsiSender::Sharing{},
                 psShare = oc::BitVector{},
                 oprfReceiver = std::make_unique<RsOprfReceiver>(),
                 cpsiSender = std::make_unique<RsCpsiSender>(),
                 psReceiver = std::make_unique<PSReceiver>(),
                 finalOtReceiver = std::make_unique<oc::SilentOtExtReceiver>(),
                 finalOtMsgs = std::vector<block>{},
                 i = u64{}
                );
        assert(mReceiverSize == X.size());

        comm = chl.bytesSent();

        oprfX.resize(X.size());
        MC_AWAIT(oprfReceiver->receive(X, oprfX, mPrng, chl));

        // comm = chl.bytesSent() - comm;
        // std::cout << "PsuReceiver::OPRF = " << (double) comm / (1 << 20) << " MB"<< std::endl;
        setTimePoint("PsuReceiver::OPRF");

        // if (mTimer) {
        //     cpsiSender->setTimer(*mTimer);
        // }
        cpsiSender->init(mSenderSize, mRecverSize, 0, mSsp, mPrng.get(), mNumThreads);
        if (mSetup) {
            cpsiSender->setTriple(mOtFactory.mMult, mOtFactory.mAdd, mOtFactory.mNumTriples);
        }
        MC_AWAIT(cpsiSender->send(oprfX, oc::Matrix<u8>(X.size(), 0), s, chl));

        // comm = chl.bytesSent() - comm;
        // std::cout << "PsuReceiver::CPSI = " << (double)comm / (1 << 20) << " MB"<< std::endl;
        setTimePoint("PsuReceiver::Cpsi send");

        // if (mTimer) {
        //     psReceiver->setTimer(*mTimer);
        // }
        psReceiver->init(s.mFlagBits.size(), mNumThreads);
        if (mSetup) {
            psReceiver->setOT(mOtFactory.mSendMsgs);
        }
        MC_AWAIT(psReceiver->runPermAndShare(s.mFlagBits, psShare, mPrng, chl));

        // comm = chl.bytesSent() - comm;
        // std::cout << "PsuReceiver::P&S = " << (double)comm / (1 << 20) << " MB" << std::endl;
        setTimePoint("PsuReceiver::PS receive");


        // if (mTimer) {
        //     finalOtReceiver->setTimer(*mTimer);
        // }
        finalOtMsgs.resize(psShare.size());
        finalOtReceiver->configure(psShare.size(), 2, mNumThreads);
        MC_AWAIT(finalOtReceiver->receiveChosen(psShare, finalOtMsgs, mPrng, chl));

        for (i = 0; i < finalOtMsgs.size(); i++) {
            if (finalOtMsgs[i] != dummyBlock) {
                D.push_back(finalOtMsgs[i]);
            }
        }

        // comm = chl.bytesSent() - comm;
        // std::cout << "PsuReceiver::OT = " << (double)comm / (1 << 20) << " MB" << std::endl;
        setTimePoint("PsuReceiver::OT recv");     

        MC_END();
    }

    Proto CpsuSender::send(span<block> Y, Socket &chl)
    {
        MC_BEGIN(Proto, this, Y, &chl,
                 oprfY = std::vector<block>{},
                 comm = u64{},
                 s = RsCpsiReceiver::Sharing{},
                 perm = std::vector<int>{},
                 invPerm = std::vector<int>{},
                 idxShareToSet = std::vector<u64>{},
                 psShare = oc::BitVector{},
                 oprfSender = std::make_unique<RsOprfSender>(),
                 cpsiReceiver = std::make_unique<RsCpsiReceiver>(),
                 psSender = std::make_unique<PSSender>(),
                 finalOtSender = std::make_unique<oc::SilentOtExtSender>(),
                 finalOtMsgs = std::vector<std::array<block, 2>>{},
                 i = u64{}
                 );

        assert(mSenderSize == Y.size());

        comm = chl.bytesSent();

        MC_AWAIT(oprfSender->send(mRecverSize, mPrng, chl));

        oprfY.resize(Y.size());
        oprfSender->eval(Y, oprfY);

        // comm = chl.bytesSent() - comm;
        // std::cout << "PsuSender::OPRF = " << (double) comm / (1 << 20) << " MB"<< std::endl;
        setTimePoint("PsuSender::OPRF");

        // if (mTimer) {
        //     cpsiReceiver->setTimer(*mTimer);
        // }
        cpsiReceiver->init(mSenderSize, mRecverSize, 0, mSsp, mPrng.get(), mNumThreads);
        if (mSetup) {
            cpsiReceiver->setTriple(mOtFactory.mMult, mOtFactory.mAdd, mOtFactory.mNumTriples);
        }
        MC_AWAIT(cpsiReceiver->receive(oprfY, s, chl));

        // comm = chl.bytesSent() - comm;
        // std::cout << "PsuSender::CPSI = " << (double) comm / (1 << 20) << " MB / output len = " << s.mFlagBits.size() << std::endl;
        setTimePoint("PsuSender::Cpsi receive");
        
        // if (mTimer) {
        //     psSender->setTimer(*mTimer);
        // }
        psSender->init(s.mFlagBits.size(), mNumThreads);
        if (mSetup) {
            psSender->setOT(mOtFactory.mRecvMsgs, mOtFactory.mRecvChoices);
        }
        MC_AWAIT(psSender->runPermAndShare(psShare, mPrng, chl));

        // comm = chl.bytesSent() - comm;
        // std::cout << "PsuSender::P&S = " << (double) comm / (1 << 20) << " MB" << std::endl;
        setTimePoint("PsuSender::PS send");

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
        
        finalOtSender->configure(perm.size(), 2, mNumThreads);
        MC_AWAIT(finalOtSender->sendChosen(finalOtMsgs, mPrng, chl));

        // comm = chl.bytesSent() - comm;
        // std::cout << "PsuSender::Final OT = " << (double) comm / (1 << 20) << " MB" << std::endl;
        setTimePoint("PsuSender::Final OT Send");  

        MC_END();
    }
}

