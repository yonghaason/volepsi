#include "RsOprf_Tests.h"
#include "volePSI/RsOprf.h"
#include "volePSI/RsOpprf.h"
#include "cryptoTools/Network/Channel.h"
#include "cryptoTools/Network/Session.h"
#include "cryptoTools/Network/IOService.h"
#include <iomanip>
#include "Common.h"

using coproto::LocalAsyncSocket;
using namespace volePSI;
using namespace oc;

void RsOprf_eval_test(const CLP& cmd)
{
    RsOprfSender sender;
    RsOprfReceiver recver;

    auto sockets = LocalAsyncSocket::makePair();
    u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
    std::random_device rd;
    std::default_random_engine gen(rd());
    std::uniform_int_distribution<u64> dis;

    PRNG prng0(block(dis(gen), dis(gen)));
    PRNG prng1(block(dis(gen), dis(gen)));

    std::vector<block> vals(n), recvOut(n);

    prng0.get(vals.data(), n);

    auto p0 = sender.send(n, prng0, sockets[0]);
    auto p1 = recver.receive(vals, recvOut, prng1, sockets[1]);

    eval(p0, p1);
    
    std::vector<block> vv(n);
    sender.eval(vals, vv);

    u64 count = 0;
    for (u64 i = 0; i < n; ++i)
    {
        auto v = sender.eval(vals[i]);
        if (recvOut[i] != v || recvOut[i]  != vv[i])
        {
            if (count < 10)
                std::cout << i << " " << recvOut[i] << " " <<v <<" " << vv[i] << std::endl;
            else
                break;

            ++count;
        }
    }
    if (count)
        throw RTE_LOC;

    std::cout << "Total Comm " 
        // << sockets[0].bytesSent() 
        // << " + " << sockets[1].bytesSent() 
        << " = " << (float) (sockets[0].bytesSent() + sockets[1].bytesSent()) / (1 << 20)
        << " MB" << std::endl;
}




void RsOprf_mal_test(const CLP& cmd)
{
    RsOprfSender sender;
    RsOprfReceiver recver;

    auto sockets = LocalAsyncSocket::makePair();

    u64 n = cmd.getOr("n", 4000);
    PRNG prng0(block(0, 0));
    PRNG prng1(block(0, 1));

    std::vector<block> vals(n), recvOut(n);

    prng0.get(vals.data(), n);

    sender.mMalicious = true;
    recver.mMalicious = true;

    auto p0 = sender.send(n, prng0, sockets[0]);
    auto p1 = recver.receive(vals, recvOut, prng1, sockets[1]);

    eval(p0, p1);
    
    std::vector<block> vv(n);
    sender.eval(vals, vv);

    u64 count = 0;
    for (u64 i = 0; i < n; ++i)
    {
        auto v = sender.eval(vals[i]);
        if (recvOut[i] != v || recvOut[i] != vv[i])
        {
            if (count < 10)
                std::cout << i << " " << recvOut[i] << " " << v << " " << vv[i] << std::endl;
            else
                break;

            ++count;
        }
    }
    if (count)
        throw RTE_LOC;
}


void RsOprf_reduced_test(const CLP& cmd)
{
    RsOprfSender sender;
    RsOprfReceiver recver;

    auto sockets = LocalAsyncSocket::makePair();

    u64 n = cmd.getOr("n", 4000);
    PRNG prng0(block(0, 0));
    PRNG prng1(block(0, 1));

    std::vector<block> vals(n), recvOut(n);

    prng0.get(vals.data(), n);

    auto p0 = sender.send(n, prng0, sockets[0], 0, true);
    auto p1 = recver.receive(vals, recvOut, prng1, sockets[1], 0, true);

    eval(p0, p1);

    std::vector<block> vv(n);
    sender.eval(vals, vv);

    u64 count = 0;
    for (u64 i = 0; i < n; ++i)
    {
        auto v = sender.eval(vals[i]);
        if (recvOut[i] != v || recvOut[i] != vv[i])
        {
            if (count < 10)
                std::cout << i << " " << recvOut[i] << " " << v << " " << vv[i] << std::endl;
            else
                break;

            ++count;
        }
    }
    if (count)
        throw RTE_LOC;

    std::cout << "Total Comm " 
        // << sockets[0].bytesSent() 
        // << " + " << sockets[1].bytesSent() 
        << " = " << (float) (sockets[0].bytesSent() + sockets[1].bytesSent()) / (1 << 20)
        << " MB" << std::endl;
}


