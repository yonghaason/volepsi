#include "RsOpprf_Tests.h"
#include "volePSI/RsOprf.h"
#include "volePSI/RsOpprf.h"
#include "cryptoTools/Network/Channel.h"
#include "cryptoTools/Network/Session.h"
#include "cryptoTools/Network/IOService.h"
#include <iomanip>
#include "Common.h"

using namespace volePSI;
using namespace oc;


void RsOpprf_eval_blk_test(const CLP& cmd)
{
    RsOpprfSender sender;
    RsOpprfReceiver recver;

    auto sockets = cp::LocalAsyncSocket::makePair();


    u64 n = cmd.getOr("n", 4000);
    std::random_device rd;
    std::default_random_engine gen(rd());
    std::uniform_int_distribution<u64> dis;

    PRNG prng0(block(dis(gen), dis(gen)));
    PRNG prng1(block(dis(gen), dis(gen)));

    std::vector<block> vals(n), out(n), recvOut(n);

    prng0.get(vals.data(), n);
    prng0.get(out.data(), n);

    auto p0 = sender.send(n, vals, out, prng0, 1, sockets[0]);
    auto p1 = recver.receive(n, vals, recvOut, prng1, 1, sockets[1]);

    eval(p0, p1);

    u64 count = 0;
    for (u64 i = 0; i < n; ++i)
    {
        auto v = sender.eval<block>(vals[i]);
        if (recvOut[i] != v || recvOut[i] != out[i])
        {
            if (count < 10)
                std::cout << i << " recv= " << recvOut[i] << ", send = " << v << ", send* = " << out[i] << std::endl;
            else
                break;

            ++count;
        }
    }
    if (count)
        throw RTE_LOC;
}

template<typename Vec>
std::string hex(const Vec& v)
{
    auto d = (u8*)v.data();
    auto s = v.size() * sizeof(typename Vec::value_type);
    std::stringstream ss;
    for (u64 i = 0; i < s; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)d[i];
    return ss.str();
}

void RsOpprf_eval_blk_mtx_test(const CLP& cmd)
{
    RsOpprfSender sender;
    RsOpprfReceiver recver;

    auto sockets = cp::LocalAsyncSocket::makePair();


    u64 n = cmd.getOr("n", 4000);
    u64 m = 7;
    std::random_device rd;
    std::default_random_engine gen(rd());
    std::uniform_int_distribution<u64> dis;

    PRNG prng0(block(dis(gen), dis(gen)));
    PRNG prng1(block(dis(gen), dis(gen)));

    std::vector<block> vals(n);
    oc::Matrix<block> out(n, m), out2(n, m), recvOut(n, m);

    prng0.get(vals.data(), n);
    prng0.get(out.data(), n);


    auto p0 = sender.send(n, vals, out, prng0, 1, sockets[0]);
    auto p1 = recver.receive(n, vals, recvOut, prng1, 1, sockets[1]);

    eval(p0, p1);
    
    sender.eval(vals, out2, 1);

    u64 count = 0;
    for (u64 i = 0; i < n; ++i)
    {
        auto c0 = memcmp(recvOut[i].data(), out[i].data(), m * sizeof(block)) != 0;
        auto c1 = memcmp(recvOut[i].data(), out2[i].data(), m * sizeof(block)) != 0;
        if (c0 || c1)
        {
            if (count < 10)
                std::cout << i << "\n\t " << hex(recvOut[i]) << "\n\t " << hex(out2[i]) << "\n\t " << hex(out[i]) << std::endl;
            else
                break;

            ++count;
        }
    }
    if (count)
        throw RTE_LOC;
}

void RsOpprf_eval_u8_test(const CLP& cmd)
{
    RsOpprfSender sender;
    RsOpprfReceiver recver;

    auto sockets = cp::LocalAsyncSocket::makePair();

    u64 n = cmd.getOr("n", 4000);
    std::random_device rd;
    std::default_random_engine gen(rd());
    std::uniform_int_distribution<u64> dis;

    PRNG prng0(block(dis(gen), dis(gen)));
    PRNG prng1(block(dis(gen), dis(gen)));

    std::vector<block> vals(n);
    std::vector<u8> out(n), recvOut(n);

    prng0.get(vals.data(), n);
    prng0.get(out.data(), n);

    auto p0 = sender.send<u8>(n, vals, out, prng0, 1, sockets[0]);
    auto p1 = recver.receive<u8>(n, vals, recvOut, prng1, 1, sockets[1]);

    eval(p0, p1);
    
    u64 count = 0;
    for (u64 i = 0; i < n; ++i)
    {
        auto v = sender.eval<u8>(vals[i]);
        if (recvOut[i] != v || recvOut[i] != out[i])
        {
            if (count < 10)
                std::cout << i << " " << recvOut[i] << " " << v << " " << out[i] << std::endl;
            else
                break;

            ++count;
        }
    }
    if (count)
        throw RTE_LOC;
}



void RsOpprf_eval_u8_mtx_test(const CLP& cmd)
{
    RsOpprfSender sender;
    RsOpprfReceiver recver;

    auto sockets = cp::LocalAsyncSocket::makePair();

    u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
    u64 m = cmd.getOr("m", 7);
    std::random_device rd;
    std::default_random_engine gen(rd());
    std::uniform_int_distribution<u64> dis;

    PRNG prng0(block(dis(gen), dis(gen)));
    PRNG prng1(block(dis(gen), dis(gen)));

    std::vector<block> vals(n);
    oc::Matrix<u8> out(n, m), out2(n, m), recvOut(n, m);

    prng0.get(vals.data(), n);
    prng0.get(out.data(), n);

    auto p0 = sender.send(n, vals, out, prng0, 1, sockets[0]);
    auto p1 = recver.receive(n, vals, recvOut, prng1, 1, sockets[1]);

    eval(p0, p1);
    

    sender.eval(vals, out2, 1);

    u64 count = 0;
    for (u64 i = 0; i < n; ++i)
    {
        auto c0 = memcmp(recvOut[i].data(), out[i].data(), m * sizeof(u8)) != 0;
        auto c1 = memcmp(recvOut[i].data(), out2[i].data(), m * sizeof(u8)) != 0;
        if (c0 || c1)
        {
            if (count < 10)
                std::cout << i << " " << hex(recvOut[i]) << " " << hex(out2[i]) << " " << hex(out[i]) << std::endl;
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

void RsOpprf_partial_test(const CLP& cmd)
{
    RsOpprfSender sender;
    RsOpprfReceiver recver;

    auto sockets = cp::LocalAsyncSocket::makePair();

    u64 nReceiver = cmd.getOr("n", 100);
    u64 nSender = cmd.getOr("ns", 0);
    if (nSender == 0) nSender = nReceiver;
    u64 m = cmd.getOr("m", 7);
    std::random_device rd;
    std::default_random_engine gen(rd());
    std::uniform_int_distribution<u64> dis;

    std::cout << "input sizes: " << nSender << ", " << nReceiver << std::endl;

    PRNG prng0(block(dis(gen), dis(gen)));
    PRNG prng1(block(dis(gen), dis(gen)));

    std::vector<block> senderKey(nSender);
    std::vector<block> recvKey(nReceiver);
    oc::Matrix<u8> senderVal(nSender, m), recvOut(nReceiver, m);

    prng0.get(senderKey.data(), nSender);
    prng0.get(recvKey.data(), nReceiver);
    prng0.get(senderVal.data(), nSender*m);
    
    std::set<u64> exp;
    for (u64 i = 0; i < nReceiver; ++i)
    {
        if (prng0.getBit())
        {
            recvKey[i] = senderKey[(i + 312) % (nSender)];
            exp.insert(i);
        }
    }

    auto p0 = sender.send(nReceiver, senderKey, senderVal, prng0, 1, sockets[0]);
    auto p1 = recver.receive(nSender, recvKey, recvOut, prng1, 1, sockets[1]);

    eval(p0, p1);
    

    u64 count = 0;
    for (u64 i = 0; i < nReceiver; ++i)
    {
        if (exp.find(i) != exp.end()) {
            auto v = senderVal[(i + 312) % (nSender)];
            auto c = memcmp(recvOut[i].data(), v.data(), m * sizeof(u8)) != 0;
            if (c)
            {
                if (count < 10)
                    std::cout << i << " " << hex(recvOut[i]) << " " << hex(v) << std::endl;
                else
                    break;

                ++count;
            }
        }
    }
    if (count)
        throw RTE_LOC;
}