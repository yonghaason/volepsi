#include "RsCpsi_Tests.h"
#include "volePSI/RsPsi.h"
#include "volePSI/RsCpsi.h"
#include "cryptoTools/Network/Channel.h"
#include "cryptoTools/Network/Session.h"
#include "cryptoTools/Network/IOService.h"
#include "Common.h"

#include <map>

using coproto::LocalAsyncSocket;
using namespace oc;
using namespace volePSI;

namespace
{

    std::vector<u64> runCpsi(
        PRNG& prng,
        std::vector<block>& recvSet,
        std::vector<block>& sendSet,
        u64 byteLength,
        u64 nt = 1,
        u64 oteBatchSize = (1ull << 25),
        ValueShareType type = ValueShareType::Xor)
    {

        std::cout << std::endl;
        auto sockets = LocalAsyncSocket::makePair();

        RsCpsiReceiver recver;
        RsCpsiSender sender;

        Timer timer, s, r;

        oc::Matrix<u8> senderValues(sendSet.size(), byteLength);
        oc::Matrix<u32> senderValues32(sendSet.size(), byteLength / sizeof(u32));
        if (type == ValueShareType::Xor && byteLength == 16) {
            std::memcpy(senderValues.data(), sendSet.data(), sendSet.size() * byteLength);
        }
        else {            
            // prng.get<u32>(senderValues32.data(), senderValues32.size());

            for (size_t i = 0; i < sendSet.size(); i++) {
                for (size_t j = 0; j < byteLength/sizeof(u32); j++) {
                    senderValues32[i][j] = i + 500;
                }
            }
            
            std::memcpy(senderValues.data(), (u8*) senderValues32.data(), sendSet.size() * byteLength);
        }

        recver.init(sendSet.size(), recvSet.size(), byteLength, 40, prng.get(), nt, oteBatchSize, type);
        sender.init(sendSet.size(), recvSet.size(), byteLength, 40, prng.get(), nt, oteBatchSize, type);

        RsCpsiReceiver::Sharing rShare;
        RsCpsiSender::Sharing sShare;

        timer.setTimePoint("start");
        
        recver.setTimer(r);
        sender.setTimer(s);

        auto p0 = recver.receive(recvSet, rShare, sockets[0]);
        auto p1 = sender.send(sendSet, senderValues, sShare, sockets[1]);

        eval(p0, p1);

        timer.setTimePoint("end");

        // std::cout << "Total Comm " 
		// 	// << sockets[0].bytesSent() 
		// 	// << " + " << sockets[1].bytesSent() 
		// 	<< " = " << (float) (sockets[0].bytesSent() + sockets[1].bytesSent()) / (1 << 20)
		// 	<< " MB" << std::endl;

        // std::cout <<"s\n" << s << "\nr" << r << std::endl;

        
        std::vector<u64> intersection;
        if (byteLength == 16 && type == ValueShareType::Xor)
        {
            std::map<block, block> sender_kv;
            for (size_t i = 0 ; i < sendSet.size(); i++) {
                sender_kv.insert({sendSet[i], *(block*)&senderValues(i, 0)});
            }
            bool failed = false;

            for (u64 i = 0; i < recvSet.size(); ++i)
            {
                auto k = rShare.mMapping[i];

                if (rShare.mFlagBits[k] ^ sShare.mFlagBits[k])
                {
                    intersection.push_back(i);

                    auto rv = *(block*)&rShare.mValues(k, 0);
                    auto sv = *(block*)&sShare.mValues(k, 0);
                    auto act = (rv ^ sv);
                    if (sender_kv[recvSet[i]] != act)
                    {
                        if(!failed)
                            std::cout << i << ": ext " << sender_kv[recvSet[i]] << ", act " << act << " = " << rv << " " << sv << std::endl;
                        failed = true;
                        throw RTE_LOC;
                    }
                }
            }
        }
        else 
        {
            std::map<block, std::vector<u32>> sender_kv;
            for (size_t i = 0 ; i < sendSet.size(); i++) {
                std::vector<u32> items(senderValues32[i].data(), senderValues32[i].data() + byteLength/sizeof(u32));
                sender_kv.insert({sendSet[i], items});
            }

            for (u64 i = 0; i < recvSet.size(); ++i)
            {
                auto k = rShare.mMapping[i];

                if (rShare.mFlagBits[k] ^ sShare.mFlagBits[k])
                {
                    intersection.push_back(i);
                    {
                        for (u64 j = 0; j < byteLength / sizeof(u32); ++j)
                        {
                            auto rv = (u32*)&rShare.mValues(k, 0);
                            auto sv = (u32*)&sShare.mValues(k, 0);

                            if (sender_kv[recvSet[i]][j] != (rv[j] + sv[j]))
                            {
                                std::cout << i << ": exp " << sender_kv[recvSet[i]][j] << ", act = " << rv[j] + sv[j] << std::endl;
                                throw RTE_LOC;
                            }
                        }
                    }
                }
            }
        }

        return intersection;
    }
}


void Cpsi_Rs_empty_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
    u64 byteLength = cmd.getOr("bytelen", 16);
    u64 oteBatchSize = 1ull << cmd.getOr("logbs", 20);
    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    prng.get(sendSet.data(), sendSet.size());

    auto inter = runCpsi(prng, recvSet, sendSet, byteLength, 1, oteBatchSize);

    if (inter.size())
        throw RTE_LOC;
}


void Cpsi_Rs_partial_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
    u64 byteLength = cmd.getOr("bytelen", 16);
    u64 oteBatchSize = 1ull << cmd.getOr("logbs", 20);
    std::vector<block> recvSet(n), sendSet(n);
    std::random_device rd;
    std::default_random_engine gen(rd());
    std::uniform_int_distribution<u64> dis;
    
    PRNG prng(block(dis(gen), dis(gen)));
    prng.get(recvSet.data(), recvSet.size());
    prng.get(sendSet.data(), sendSet.size());

    std::set<u64> exp;
    for (u64 i = 0; i < n; ++i)
    {
        if (prng.getBit())
        {
            recvSet[i] = sendSet[(i + 312) % n];
            exp.insert(i);
        }
    }

    auto inter = runCpsi(prng, recvSet, sendSet, byteLength, 1, oteBatchSize);
    std::set<u64> act(inter.begin(), inter.end());
    if (act != exp)
    {   
        std::cout << "derived inter size " << act.size(); 
        std::cout << ", expected to be " << exp.size() << std::endl;
        auto rem = exp;
        for (auto a : act)
            rem.erase(a);

        std::cout << "missing " << *rem.begin() << " " << recvSet[*rem.begin()] << std::endl;
        throw RTE_LOC;
    }
}


void Cpsi_Rs_full_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
    u64 byteLength = cmd.getOr("bytelen", 16);
    u64 oteBatchSize = 1ull << (cmd.getOr("logbs", 20));
    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    for (size_t i = 0; i < sendSet.size(); i++) 
    {
        sendSet[i] = recvSet[recvSet.size() - 1 - i];
    }

    std::set<u64> exp;
    for (u64 i = 0; i < n; ++i)
        exp.insert(i);

    auto inter = runCpsi(prng, recvSet, sendSet, byteLength, 1, oteBatchSize);
    std::set<u64> act(inter.begin(), inter.end());
    if (act != exp)
        throw RTE_LOC;
}

void Cpsi_Rs_full_asym_test(const CLP& cmd)
{
    u64 ns = cmd.getOr("ns", 2432);
    u64 nr = cmd.getOr("nr", 212);
    u64 byteLength = cmd.getOr("bytelen", 16);
    u64 oteBatchSize = 1ull << cmd.getOr("logbs", 20);
    std::vector<block> recvSet(nr), sendSet(ns);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    prng.SetSeed(ZeroBlock);
    prng.get(sendSet.data(), sendSet.size());

    std::set<u64> exp;
    for (u64 i = 0; i < std::min<u64>(ns,nr); ++i)
        exp.insert(i);

    auto inter = runCpsi(prng, recvSet, sendSet, byteLength, 1, oteBatchSize);
    std::set<u64> act(inter.begin(), inter.end());
    if (act != exp)
        throw RTE_LOC;    
}

void Cpsi_Rs_full_add32_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
    u64 byteLength = cmd.getOr("bytelen", 16);
    u64 oteBatchSize = 1ull << cmd.getOr("logbs", 20);

    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    for (size_t i = 0; i < sendSet.size(); i++) 
    {
        sendSet[i] = recvSet[recvSet.size() - 1 - i];
    }
    
    std::set<u64> exp;
    for (u64 i = 0; i < n; ++i)
       exp.insert(i);

    auto inter = runCpsi(prng, recvSet, sendSet, byteLength, 1, oteBatchSize, ValueShareType::add32);
    std::set<u64> act(inter.begin(), inter.end());
    if (act != exp)
       throw RTE_LOC;
}