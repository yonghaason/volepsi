//
#include "GMW_Tests.h"
#include "volePSI/Defines.h"
#include "volePSI/GMW/Gmw.h"
#include "volePSI/GMW/SilentOteGen.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Network/Session.h"
#include "cryptoTools/Circuit/BetaLibrary.h"
#include "Common.h"
#include "coproto/Socket/LocalAsyncSock.h"
#include <bitset>

#include <unistd.h>

using namespace volePSI;
using coproto::LocalAsyncSocket;

using PRNG = oc::PRNG;

void SilentOteGen_test(const oc::CLP& cmd)
{
    u64 numot = cmd.getOr("numot", 14988321);
    u64 numtri = cmd.getOr("numtri", 4358972139);
    u64 bs = 1ull << cmd.getOr("logbs", 20);
    u64 nt = 1;
    //Mode mode = Mode::Dual;

    SilentOteGen t0, t1;
    auto sockets = LocalAsyncSocket::makePair();

    t0.init(numot, numtri, bs, nt, Mode::Receiver, oc::ZeroBlock);
    t1.init(numot, numtri, bs, nt, Mode::Sender, oc::OneBlock);

    auto b0 = t0.requiredBaseOts();
    auto b1 = t1.requiredBaseOts();

    if (b0.mNumSend != b1.mRecvChoiceBits.size() ||
        b1.mNumSend != b0.mRecvChoiceBits.size())
        throw RTE_LOC;

    std::vector<block> m0(b0.mRecvChoiceBits.size()), m1(b1.mRecvChoiceBits.size());
    std::vector<std::array<block, 2>> mm0(b0.mNumSend), mm1(b1.mNumSend);

    oc::PRNG prng(oc::CCBlock);
    prng.get(mm0.data(), mm0.size());
    prng.get(mm1.data(), mm1.size());
    for (u64 i = 0; i < mm0.size(); ++i)
        m1[i] = mm0[i][b1.mRecvChoiceBits[i]];
    for (u64 i = 0; i < mm1.size(); ++i)
        m0[i] = mm1[i][b0.mRecvChoiceBits[i]];
    t0.setBaseOts(m0, mm0);
    t1.setBaseOts(m1, mm1);

    auto p0 = t0.expand(sockets[0]);
    auto p1 = t1.expand(sockets[1]);
    eval(p0, p1);
    
    for (u64 i = 0; i < static_cast<u64>(t0.mMult.size()); ++i)
    {
        if (neq((t0.mMult[i] & t1.mMult[i]) ^ t0.mAdd[i], t1.mAdd[i]))
            throw std::runtime_error("");

        //if (neq((t1.mA[i] & t0.mC[i]) ^ t1.mB[i], t0.mD[i]))
        //    throw std::runtime_error("");
    }
    
    std::cout << "Total Comm " 
        // << sockets[0].bytesSent() 
        // << " + " << sockets[1].bytesSent() 
        << " = " << (float) (sockets[0].bytesSent() + sockets[1].bytesSent()) / (1 << 20)
        << " MB" << std::endl;
}

void baseOT_Test(const oc::CLP& cmd)
{

    u64 n = cmd.getOr("n", 1ull << 20);
    u64 bs = cmd.getOr("bs", 1ull << 25);
    u64 nt = 1;
    
    SilentOteGen t0, t1;
    auto sockets = LocalAsyncSocket::makePair();

    PRNG prng0(oc::ZeroBlock);
    PRNG prng1(oc::OneBlock);

    t0.init(n, bs, nt, Mode::Receiver, prng0.get());
    t1.init(n, bs, nt, Mode::Sender, prng1.get());

    auto p0 = t0.generateBaseOts(1, prng0, sockets[0]);
    auto p1 = t1.generateBaseOts(0, prng1, sockets[1]);

    eval(p0, p1);
    
    std::cout << "BaseOT Comm: " 
            << sockets[0].bytesSent() 
            << " + " << sockets[1].bytesSent() 
            << " = " << (sockets[0].bytesSent() + sockets[1].bytesSent())
            << " bytes" << std::endl;          
    
    u64 comm0 = sockets[0].bytesSent(), comm1 = sockets[1].bytesSent();

    auto p02 = t0.expand(sockets[0]);
    auto p12 = t1.expand(sockets[1]);

    eval(p02, p12);
    
    std::cout << "Expand Comm: " 
            << sockets[0].bytesSent() -comm0
            << " + " << sockets[1].bytesSent() - comm1
            << " = " << (sockets[0].bytesSent() + sockets[1].bytesSent()) - comm0 - comm1
            << " bytes" << std::endl;
}


void makeTriple(span<block> a, span<block> b, span<block> c, span<block> d, oc::PRNG& prng)
{
    prng.get(a.data(), a.size());
    prng.get(b.data(), b.size());
    prng.get(c.data(), c.size());
    for (u64 i = 0; i < static_cast<u64>(a.size()); i++)
    {
        d[i] = (a[i] & c[i]) ^ b[i];
    }
}



void Gmw_half_test(const oc::CLP& cmd)
{
    //oc::IOService ios;
    //coproto::Socket sockets[0] = oc::Session(ios, "localhost:1212", oc::SessionMode::Server).addChannel();
    //coproto::Socket sockets[1] = oc::Session(ios, "localhost:1212", oc::SessionMode::Client).addChannel();
    auto sockets = LocalAsyncSocket::makePair();

    Gmw cmp0, cmp1;
    block seed = oc::toBlock(cmd.getOr<u64>("s", 0));
    oc::PRNG prng(seed);

    u64 n = cmd.getOr("n", 100ull);
    u64 bc = cmd.getOr("bc", 16);
    //        u64 eqSize = cmd.getOr("e", n / 2);


    std::vector<block> input0(n), input1(n), z0(n), z1(n);

    prng.get(input0.data(), input0.size());
    prng.get(input1.data(), input1.size());

    auto cir = isZeroCircuit(bc);
    cmp0.init(n * 128, cir, 1, 0, oc::ZeroBlock);
    cmp1.init(n * 128, cir, 1, 1, oc::OneBlock);

    std::vector<block> a0(n), b0(n), c0(n), d0(n);
    std::vector<block> a1(n), b1(n), c1(n), d1(n);
    makeTriple(a0, b0, c0, d0, prng);
    makeTriple(a1, b1, c1, d1, prng);
    cmp0.setTriples(a0, b0, c1, d1);
    cmp1.setTriples(a1, b1, c0, d0);

    auto p0 = cmp0.multSendP1(input0, sockets[0], oc::GateType::And);
    auto p1 = cmp1.multRecvP2(input1, z1, sockets[1]);
    eval(p0, p1);
    
    p0 = cmp0.multRecvP1(input0, z0, sockets[0], oc::GateType::And);
    p1 = cmp1.multSendP2(input1, sockets[1], oc::GateType::And);
    eval(p0, p1);
    
    for (u64 i = 0; i < n; i++)
    {
        auto exp = (input0[i] & input1[i]);
        auto act = z0[i] ^ z1[i];
        if (neq(act, exp))
        {
            oc::lout << "exp " << exp << std::endl;
            oc::lout << "act " << act << std::endl;
            oc::lout << "dif " << (act ^ exp) << std::endl;
            throw RTE_LOC;
        }
    }
}

void Gmw_basic_test(const oc::CLP& cmd)
{
    auto sockets = LocalAsyncSocket::makePair();

    Gmw cmp0, cmp1;
    block seed = oc::toBlock(cmd.getOr<u64>("s", 0));
    oc::PRNG prng(seed);

    u64 n = cmd.getOr("n", 100ull);
    u64 bc = cmd.getOr("bc", 16);


    std::array<std::vector<block>, 2> x, y;
    std::array<std::vector<block>, 2> z;
    x[0].resize(n);
    x[1].resize(n);
    y[0].resize(n);
    y[1].resize(n);
    z[0].resize(n);
    z[1].resize(n);

    for (u64 i = 0; i < 2; ++i)
    {
        prng.get(x[i].data(), n);
        prng.get(y[i].data(), n);
    }

    auto cir = isZeroCircuit(bc);
    cmp0.init(n * 128, cir, 1, 0, oc::ZeroBlock);
    cmp1.init(n * 128, cir, 1, 1, oc::OneBlock);

    std::vector<block> a0(n), b0(n), c0(n), d0(n);
    std::vector<block> a1(n), b1(n), c1(n), d1(n);
    makeTriple(a0, b0, c0, d0, prng);
    makeTriple(a1, b1, c1, d1, prng);
    cmp0.setTriples(a0, b0, c1, d1);
    cmp1.setTriples(a1, b1, c0, d0);


    auto p0 = cmp0.multSendP1(x[0], y[0], sockets[0], oc::GateType::And);
    auto p1 = cmp1.multRecvP2(x[1], y[1], z[1], sockets[1]);
    eval(p0, p1);
    
    p0 = cmp0.multRecvP1(x[0], y[0], z[0], sockets[0], oc::GateType::And);
    p1 = cmp1.multSendP2(x[1], y[1], sockets[1]);
    eval(p0, p1);
    
    for (u64 i = 0; i < n; i++)
    {
        auto exp =
            (x[0][i] ^ x[1][i]) &
            (y[0][i] ^ y[1][i]);

        auto act = z[0][i] ^ z[1][i];
        if (neq(act, exp))
        {
            oc::lout << "exp " << exp << std::endl;
            oc::lout << "act " << act << std::endl;
            oc::lout << "dif " << (act ^ exp) << std::endl;
            throw RTE_LOC;
        }
    }
}

void trim(std::vector<block>& v, u64 l)
{
    if (l > 128)
        throw RTE_LOC;
    block mask = oc::ZeroBlock;
    oc::BitIterator iter((u8*)&mask, 0);
    for (u64 i = 0; i < l; i++)
        *iter++ = 1;

    //oc::lout << "mask " << l << " " << mask << std::endl;
    for (u64 i = 0; i < v.size(); i++)
    {
        v[i] = v[i] & mask;
    }
}

std::vector<u64> randomSubset(u64 n, u64 m, oc::PRNG& prng)
{
    std::vector<u64> set(n), ret(m);
    std::iota(set.begin(), set.end(), 0);

    for (u64 i = 0; i < m; i++)
    {
        auto j = prng.get<u64>() % set.size();
        ret[i] = set[j];
        std::swap(set[j], set.back());
        set.pop_back();
    }
    return ret;
}

std::array<Matrix<u8>, 2> share(span<i64> v, PRNG& prng)
{
    auto n = v.size();
    Matrix<u8>
        s0(n, sizeof(u64)),
        s1(n, sizeof(u64));

    prng.get(s0.data(), s0.size());

    for (u64 i = 0; i < n; ++i)
    {
        auto& i0s0 = *(i64*)s0[i].data();
        auto& i0s1 = *(i64*)s1[i].data();

        i0s1 = v[i] ^ i0s0;
    }

    return { s0, s1 };
}


std::array<Matrix<u8>, 2> share(Matrix<u8> v, PRNG& prng)
{
    auto n = v.rows();
    Matrix<u8>
        s0(n, v.cols()),
        s1(n, v.cols());

    prng.get(s0.data(), s0.size());

    for (u64 i = 0; i < v.size(); ++i)
        s1(i) = v(i) ^ s0(i);

    return { s0, s1 };
}


template <typename T>
std::vector<T> reconstruct(std::array<Matrix<u8>, 2> shares)
{
    if (shares[0].cols() != sizeof(T))
        throw RTE_LOC;
    if (shares[1].cols() != sizeof(T))
        throw RTE_LOC;
    if (shares[0].rows() != shares[1].rows())
        throw RTE_LOC;

    std::vector<T> ret(shares[0].rows());
    oc::MatrixView<u8> v((u8*)ret.data(), ret.size(), sizeof(T));

    for (u64 i = 0; i < v.size(); ++i)
        v(i) = shares[0](i) ^ shares[1](i);

    return ret;
}

void Gmw_inOut_test(const oc::CLP& cmd)
{

    u64 w = 64;
    u64 n = 100;
    BetaCircuit cir;
    BetaBundle io(w);

    cir.addInputBundle(io);
    cir.mOutputs.push_back(io);

    Gmw gmw;
    gmw.init(n, cir, 1, 0, oc::ZeroBlock);

    Matrix<u8> in(n, oc::divCeil(w, 8));
    Matrix<u8> out(n, oc::divCeil(w, 8));

    PRNG prng(block(0, 0));
    prng.get(in.data(), in.size());
    gmw.setInput(0, in);
    gmw.getOutput(0, out);

    if (!(in == out))
        throw RTE_LOC;
}

void Gmw_xor_test(const oc::CLP& cmd)
{

    u64 w = 64;
    u64 n = 100;

    BetaCircuit cir = *oc::BetaLibrary().int_int_bitwiseXor(w, w, w);

    Gmw gmw;
    gmw.init(n, cir, 1, 0, oc::ZeroBlock);

    Matrix<u8> in0(n, oc::divCeil(w, 8));
    Matrix<u8> in1(n, oc::divCeil(w, 8));
    Matrix<u8> out(n, oc::divCeil(w, 8));

    PRNG prng(block(0, 0));
    prng.get(in0.data(), in0.size());
    prng.get(in1.data(), in1.size());

    gmw.setInput(0, in0);
    gmw.setInput(1, in1);
    coproto::Socket chl;
    macoro::sync_wait(gmw.run(chl));
    gmw.getOutput(0, out);

    for (u64 i = 0; i < out.size(); ++i)
        if (!((in0(i) ^ in1(i)) == out(i)))
            throw RTE_LOC;
}


void Gmw_and_test(const oc::CLP& cmd)
{

    u64 w = 64;
    u64 n = 100;

    auto sockets = LocalAsyncSocket::makePair();


    BetaCircuit cir = *oc::BetaLibrary().int_int_bitwiseAnd(w, w, w);

    Gmw gmw0, gmw1;
    gmw0.init(n, cir, 1, 0, oc::ZeroBlock);
    gmw1.init(n, cir, 1, 1, oc::ZeroBlock);

    gmw0.mO.mDebug = true;
    gmw1.mO.mDebug = true;

    Matrix<u8> in0(n, oc::divCeil(w, 8));
    Matrix<u8> in1(n, oc::divCeil(w, 8));
    Matrix<u8> out0(n, oc::divCeil(w, 8));
    Matrix<u8> out1(n, oc::divCeil(w, 8));

    PRNG prng(block(0, 0));
    prng.get(in0.data(), in0.size());
    prng.get(in1.data(), in1.size());

    auto sin0 = share(in0, prng);
    auto sin1 = share(in1, prng);

    gmw0.setInput(0, sin0[0]);
    gmw0.setInput(1, sin1[0]);
    gmw1.setInput(0, sin0[1]);
    gmw1.setInput(1, sin1[1]);

    auto p0 = gmw0.run(sockets[0]);
    auto p1 = gmw1.run(sockets[1]);
    eval(p0, p1);
    
    gmw0.getOutput(0, out0);
    gmw1.getOutput(0, out1);

    for (u64 i = 0; i < out0.size(); ++i)
    {
        auto exp = in0(i) & in1(i);
        auto act = out0(i) ^ out1(i);

        if (exp != act)
            throw RTE_LOC;
    }
}


void Gmw_na_and_test(const oc::CLP& cmd)
{

    u64 w = 8;
    u64 n = 1;

    auto sockets = LocalAsyncSocket::makePair();


    BetaCircuit cir;

    BetaBundle a(w);
    BetaBundle b(w);
    BetaBundle t0(w);
    BetaBundle t1(w);
    BetaBundle c(w);
    BetaBundle d(w);

    cir.addInputBundle(a);
    cir.addInputBundle(b);
    //cir.addTempWireBundle(t0);
    //cir.addTempWireBundle(t1);
    cir.addOutputBundle(c);
    //cir.addOutputBundle(d);

    for (u64 i = 0; i < w; ++i)
    {
        cir.addGate(a[i], b[i], oc::GateType::na_And, c[i]);
        //cir.addCopy(a[i], t0[i]);
        //cir.addGate(a[i], b[i], oc::GateType::Xor, t1[i]);
        //cir.addGate(t1[i], z[i], oc::GateType::na_And, d[i]);
    }

    Gmw gmw0, gmw1;
    gmw0.init(n, cir, 1, 0, oc::ZeroBlock);
    gmw1.init(n, cir, 1, 1, oc::ZeroBlock);

    gmw0.mO.mDebug = true;
    gmw1.mO.mDebug = true;

    Matrix<u8> in0(n, oc::divCeil(w, 8));
    Matrix<u8> in1(n, oc::divCeil(w, 8));
    Matrix<u8> out0(n, oc::divCeil(w, 8));
    Matrix<u8> out1(n, oc::divCeil(w, 8));
    Matrix<u8> d0(n, oc::divCeil(w, 8));
    Matrix<u8> d1(n, oc::divCeil(w, 8));

    PRNG prng(block(0, 0));
    prng.get(in0.data(), in0.size());
    prng.get(in1.data(), in1.size());

    auto sin0 = share(in0, prng);
    auto sin1 = share(in1, prng);

    gmw0.setInput(0, sin0[0]);
    gmw0.setInput(1, sin1[0]);
    gmw1.setInput(0, sin0[1]);
    gmw1.setInput(1, sin1[1]);



    auto p0 = gmw0.run(sockets[0]);
    auto p1 = gmw1.run(sockets[1]);
    eval(p0, p1);
    
    gmw0.getOutput(0, out0);
    gmw1.getOutput(0, out1);


    oc::RandomOracle ro(16);
    ro.Update(out0.data(), out0.size());
    ro.Update(out1.data(), out0.size());
    block bb;
    ro.Final(bb);
    //std::cout << "\n\n\n\nb " << bb << std::endl;
    //gmw0.getOutput(1, d0);
    //gmw1.getOutput(1, d1);

    for (u64 i = 0; i < out0.size(); ++i)
    {
        {
            auto exp = ~in0(i) & in1(i);
            auto act = out0(i) ^ out1(i);

            if (exp != act)
                throw RTE_LOC;
        }
        //{

        //    auto z = ~in0(i) & in1(i);
        //    auto t = in0(i) ^ in1(i);
        //    auto exp = ~t & z;
        //    auto act = d0(i) ^ d1(i);

        //    if (exp != act)
        //        throw RTE_LOC;
        //}
    }
}

void Gmw_or_test(const oc::CLP& cmd)
{

    u64 w = 64;
    u64 n = 100;

    auto sockets = LocalAsyncSocket::makePair();

    BetaCircuit cir;

    BetaBundle a(w);
    BetaBundle b(w);
    BetaBundle c(w);

    cir.addInputBundle(a);
    cir.addInputBundle(b);
    cir.addOutputBundle(c);

    for (u64 i = 0; i < w; ++i)
        cir.addGate(a[i], b[i], oc::GateType::Or, c[i]);

    Gmw gmw0, gmw1;
    gmw0.init(n, cir, 1, 0, oc::ZeroBlock);
    gmw1.init(n, cir, 1, 1, oc::ZeroBlock);

    gmw0.mO.mDebug = true;
    gmw1.mO.mDebug = true;

    Matrix<u8> in0(n, oc::divCeil(w, 8));
    Matrix<u8> in1(n, oc::divCeil(w, 8));
    Matrix<u8> out0(n, oc::divCeil(w, 8));
    Matrix<u8> out1(n, oc::divCeil(w, 8));

    PRNG prng(block(0, 0));
    prng.get(in0.data(), in0.size());
    prng.get(in1.data(), in1.size());

    auto sin0 = share(in0, prng);
    auto sin1 = share(in1, prng);

    gmw0.setInput(0, sin0[0]);
    gmw0.setInput(1, sin1[0]);
    gmw1.setInput(0, sin0[1]);
    gmw1.setInput(1, sin1[1]);

    auto p0 = gmw0.run(sockets[0]);
    auto p1 = gmw1.run(sockets[1]);
    eval(p0, p1);
    
    gmw0.getOutput(0, out0);
    gmw1.getOutput(0, out1);

    for (u64 i = 0; i < out0.size(); ++i)
    {
        auto exp = in0(i) | in1(i);
        auto act = out0(i) ^ out1(i);

        if (exp != act)
            throw RTE_LOC;
    }
}


void Gmw_xor_and_test(const oc::CLP& cmd)
{

    u64 w = 8;
    u64 n = 1;

    auto sockets = LocalAsyncSocket::makePair();

    BetaCircuit cir;

    BetaBundle a(w);
    BetaBundle b(w);
    BetaBundle c(w);
    BetaBundle t0(w);
    BetaBundle t1(w);
    BetaBundle z(w);

    cir.addInputBundle(a);
    cir.addInputBundle(b);
    cir.addInputBundle(c);
    cir.addTempWireBundle(t0);
    cir.addTempWireBundle(t1);
    cir.addOutputBundle(z);

    for (u64 i = 0; i < w; ++i)
    {
        cir.addGate(a[i], c[i], oc::GateType::Xor, t0[i]);
        //cir.addCopy(t0[i], t1[i]);
        cir.addGate(t0[i], b[i], oc::GateType::And, z[i]);
    }

    Gmw gmw0, gmw1;
    gmw0.mLevelize = BetaCircuit::LevelizeType::Reorder;
    gmw1.mLevelize = BetaCircuit::LevelizeType::Reorder;

    gmw0.init(n, cir, 1, 0, oc::ZeroBlock);
    gmw1.init(n, cir, 1, 1, oc::ZeroBlock);

    gmw0.mO.mDebug = true;
    gmw1.mO.mDebug = true;

    Matrix<u8> in0(n, oc::divCeil(w, 8));
    Matrix<u8> in1(n, oc::divCeil(w, 8));
    Matrix<u8> in2(n, oc::divCeil(w, 8));
    Matrix<u8> out0(n, oc::divCeil(w, 8));
    Matrix<u8> out1(n, oc::divCeil(w, 8));
    Matrix<u8> d0(n, oc::divCeil(w, 8));
    Matrix<u8> d1(n, oc::divCeil(w, 8));

    PRNG prng(block(0, 0));
    prng.get(in0.data(), in0.size());
    prng.get(in1.data(), in1.size());
    prng.get(in2.data(), in2.size());

    auto sin0 = share(in0, prng);
    auto sin1 = share(in1, prng);
    auto sin2 = share(in2, prng);

    gmw0.setInput(0, sin0[0]);
    gmw0.setInput(1, sin1[0]);
    gmw0.setInput(2, sin2[0]);

    gmw1.setInput(0, sin0[1]);
    gmw1.setInput(1, sin1[1]);
    gmw1.setInput(2, sin2[1]);



    auto p0 = gmw0.run(sockets[0]);
    auto p1 = gmw1.run(sockets[1]);
    eval(p0, p1);
    
    gmw0.getOutput(0, out0);
    gmw1.getOutput(0, out1);

    for (u64 i = 0; i < out0.size(); ++i)
    {
        {
            auto exp = (in0(i) ^ in2(i)) & in1(i);
            auto act = out0(i) ^ out1(i);

            if (exp != act)
                throw RTE_LOC;
        }
    }
}

void Gmw_aa_na_and_test(const oc::CLP& cmd)
{

    u64 w = cmd.getOr("w", 40);
    u64 n = cmd.getOr("n", 128);

    auto sockets = LocalAsyncSocket::makePair();

    BetaCircuit cir;

    BetaBundle a(w);
    BetaBundle b(w);
    BetaBundle t0(w);
    BetaBundle t1(w);
    BetaBundle z(w);

    cir.addInputBundle(a);
    cir.addInputBundle(b);
    cir.addTempWireBundle(t0);
    cir.addTempWireBundle(t1);
    cir.addOutputBundle(z);

    for (u64 i = 0; i < w; ++i)
    {
        cir.addCopy(a[i], t0[i]);
        cir.addCopy(t0[i], t1[i]);
        cir.addGate(t0[i], b[i], oc::GateType::na_And, z[i]);
    }

    Gmw gmw0, gmw1;
    gmw0.mLevelize = BetaCircuit::LevelizeType::NoReorder;
    gmw1.mLevelize = BetaCircuit::LevelizeType::NoReorder;

    gmw0.init(n, cir, 1, 0, oc::ZeroBlock);
    gmw1.init(n, cir, 1, 1, oc::ZeroBlock);

    gmw0.mO.mDebug = true;
    gmw1.mO.mDebug = true;

    Matrix<u8> in0(n, oc::divCeil(w, 8));
    Matrix<u8> in1(n, oc::divCeil(w, 8));
    Matrix<u8> out0(n, oc::divCeil(w, 8));
    Matrix<u8> out1(n, oc::divCeil(w, 8));

    PRNG prng(block(0, 0));
    prng.get(in0.data(), in0.size());
    prng.get(in1.data(), in1.size());

    u8 mask = ~0;
    if (w & 7)
    {
        mask = (1 << (w & 7)) - 1;
        for (u64 i = 0; i < n; ++i)
        {
            in0(i, in0.cols() - 1) &= mask;
            in1(i, in1.cols() - 1) &= mask;
        }
    }

    auto sin0 = share(in0, prng);
    auto sin1 = share(in1, prng);

    gmw0.setInput(0, sin0[0]);
    gmw0.setInput(1, sin1[0]);

    gmw1.setInput(0, sin0[1]);
    gmw1.setInput(1, sin1[1]);

    auto p0 = gmw0.run(sockets[0]); 
    auto p1 = gmw1.run(sockets[1]);
    eval(p0, p1);
    
    gmw0.getOutput(0, out0);
    gmw1.getOutput(0, out1);


    for (u64 i = 0; i < n; ++i)
    {
        for (u64 j = 0; j < in0.cols(); ++j)
        {
            u8 m = j == in0.cols() - 1 ? mask : ~u8(0);
            auto exp = (~in0(i, j) & m) & in1(i, j);
            auto act = out0(i, j) ^ out1(i, j);

            if (exp != act)
            {
                std::cout << "exp: " << exp << " act: " << act << std::endl;
                throw RTE_LOC;
            }
        }
    }
}

void Gmw_add_test(const oc::CLP& cmd)
{
    oc::BetaLibrary lib;

    auto addCir = lib.int_int_add(64, 64, 64);

    auto sockets = LocalAsyncSocket::makePair();

    Gmw gmw0, gmw1;
    block seed = oc::toBlock(cmd.getOr<u64>("s", 0));
    oc::PRNG prng(seed);

    u64 n = 100;

    std::vector<i64> in0(n), in1(n), out(n);
    std::array<Matrix<u8>, 2> sout;
    sout[0].resize(n, sizeof(i64));
    sout[1].resize(n, sizeof(i64));

    prng.get(in0.data(), n);
    prng.get(in1.data(), n);
    auto sin0 = share(in0, prng);
    auto sin1 = share(in1, prng);

    //gmw0.mO.mDebug = true;
    //gmw1.mO.mDebug = true;
    gmw0.init(n, *addCir, 1, 0, oc::ZeroBlock);
    gmw1.init(n, *addCir, 1, 1, oc::OneBlock);

    gmw0.setInput(0, sin0[0]);
    gmw0.setInput(1, sin1[0]);
    gmw1.setInput(0, sin0[1]);
    gmw1.setInput(1, sin1[1]);

    auto p0 = gmw0.run(sockets[0]); 
    auto p1 = gmw1.run(sockets[1]);
    eval(p0, p1);
    
    gmw0.getOutput(0, sout[0]);
    gmw1.getOutput(0, sout[1]);

    out = reconstruct<i64>(sout);

    for (u64 i = 0; i < n; ++i)
    {
        auto exp = (in0[i] + in1[i]);
        auto act = out[i];
        if (act != exp)
        {
            std::cout << "i   " << i << std::endl;
            std::cout << "exp " << exp << std::endl;
            std::cout << "act " << act << std::endl;
            throw RTE_LOC;
        }
    }
}

void Gmw_noLevelize_test(const oc::CLP& cmd)
{
    oc::BetaLibrary lib;

    auto addCir = lib.int_int_add(64, 64, 64);

    auto sockets = LocalAsyncSocket::makePair();

    Gmw gmw0, gmw1;
    block seed = oc::toBlock(cmd.getOr<u64>("s", 0));
    oc::PRNG prng(seed);

    u64 n = 100;

    std::vector<i64> in0(n), in1(n), out(n);
    std::array<Matrix<u8>, 2> sout;
    sout[0].resize(n, sizeof(i64));
    sout[1].resize(n, sizeof(i64));

    prng.get(in0.data(), n);
    prng.get(in1.data(), n);
    auto sin0 = share(in0, prng);
    auto sin1 = share(in1, prng);

    gmw0.mLevelize = BetaCircuit::LevelizeType::NoReorder;
    gmw1.mLevelize = BetaCircuit::LevelizeType::NoReorder;
    gmw0.init(n, *addCir, 1, 0, oc::ZeroBlock);
    gmw1.init(n, *addCir, 1, 1, oc::OneBlock);


    gmw0.setInput(0, sin0[0]);
    gmw0.setInput(1, sin1[0]);
    gmw1.setInput(0, sin0[1]);
    gmw1.setInput(1, sin1[1]);

    auto p0 = gmw0.run(sockets[0]);
    auto p1 = gmw1.run(sockets[1]);
    eval(p0, p1);
    
    gmw0.getOutput(0, sout[0]);
    gmw1.getOutput(0, sout[1]);

    out = reconstruct<i64>(sout);

    for (u64 i = 0; i < n; ++i)
    {
        auto exp = (in0[i] + in1[i]);
        auto act = out[i];
        if (act != exp)
        {
            std::cout << "i   " << i << std::endl;
            std::cout << "exp " << exp << std::endl;
            std::cout << "act " << act << std::endl;
            throw RTE_LOC;
        }
    }
}

void printAsBinary(uint8_t value) {
    // Use std::bitset to convert the uint8_t value to an 8-bit binary string
    std::bitset<8> binary(value);
    std::cout << binary << std::endl;
}

void Gmw_thr_test(const oc::CLP& cmd)
{
    oc::BetaLibrary lib;

    u64 bitlen = 12;
    u64 bytelen = oc::divCeil(bitlen, 8);

    auto addCir = lib.int_int_add(bitlen, bitlen, bitlen);
    
    auto sockets = LocalAsyncSocket::makePair();

    Gmw gmw0, gmw1;
    block seed = oc::toBlock(cmd.getOr<u64>("s", 0));
    oc::PRNG prng(seed);

    u64 n = 1;

    Matrix<u8> in0(n, bytelen), in1(n, bytelen), out(n, bytelen);
    std::array<Matrix<u8>, 2> sadd;
    sadd[0].resize(n, bytelen);
    sadd[1].resize(n, bytelen);

    auto a = prng.get<u64>() % (1ull<<bitlen);
    auto b = prng.get<u64>() % (1ull<<bitlen);

    memcpy(in0[0].data(), &a, bytelen);
    memcpy(in1[0].data(), &b, bytelen);
    
    gmw0.init(n, *addCir, 1, 0, oc::ZeroBlock);
    gmw1.init(n, *addCir, 1, 1, oc::OneBlock);

    gmw0.setInput(0, in0);
    gmw0.setInput(1, Matrix<u8>(n, bytelen));
    
    gmw1.setInput(0, Matrix<u8>(n, bytelen));
    gmw1.setInput(1, in1);

    auto p0 = gmw0.run(sockets[0]); 
    auto p1 = gmw1.run(sockets[1]);
    eval(p0, p1);
    
    gmw0.getOutput(0, sadd[0]);
    gmw1.getOutput(0, sadd[1]);

    std::cout << "Add: ";
    for (size_t i = 0; i < bytelen; i++) 
    {
        std::cout << std::bitset<8>(sadd[0][0][bytelen - 1 - i] ^ sadd[1][0][bytelen - 1 - i]);
    }
    std::cout << std::endl;

    auto gtrCir = lib.int_int_gteq(bitlen, bitlen);
    auto thr = prng.get<u64>() % (1ull<<bitlen);

    Matrix<u8> inthr(n, bytelen);
    memcpy(inthr[0].data(), &thr, bytelen);

    gmw0.init(n, *gtrCir, 1, 0, oc::ZeroBlock);
    gmw1.init(n, *gtrCir, 1, 1, oc::OneBlock);
    
    gmw0.setInput(0, sadd[0]);
    gmw0.setInput(1, inthr);

    gmw1.setInput(0, sadd[1]);
    gmw1.setInput(1, Matrix<u8>(n, bytelen));

    p0 = gmw0.run(sockets[0]); 
    p1 = gmw1.run(sockets[1]);
    eval(p0, p1);

    std::array<Matrix<u8>, 2> sthr;
    sthr[0].resize(n, 1);
    sthr[1].resize(n, 1);
    
    gmw0.getOutput(0, sthr[0]);
    gmw1.getOutput(0, sthr[1]);

    std::cout << "Thr out: ";
    std::cout << std::bitset<8>(sthr[0][0][0] ^ sthr[1][0][0]);
    std::cout << std::endl;

    std::cout << "a, b, t: " << a << ", " << b << ", " << thr << ", " << std::endl;
    std::cout << "a + b: " << (a + b) % (1 << bitlen) << std::endl;
}


