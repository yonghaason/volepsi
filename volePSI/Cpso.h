#pragma once

#include "volePSI/Defines.h"
#include "volePSI/config.h"
#include "volePSI/RsCpsi.h"
#ifdef VOLE_PSI_ENABLE_CPSI

#include "cryptoTools/Crypto/PRNG.h"
#include "cryptoTools/Network/Channel.h"
#include "cryptoTools/Common/Timer.h"

namespace volePSI
{
    class CpsuSender : public details::RsCpsiBase, public oc::TimerAdapter
    {
    public:
        block dummyBlock = oc::AllOneBlock;
        Proto send(span<block> Y, Socket& chl);
    };

    class CpsuReceiver : public details::RsCpsiBase, public oc::TimerAdapter
    {
    public:
        block dummyBlock = oc::AllOneBlock;
        Proto receive(span<block> X, std::vector<block>& D, Socket& chl);
    };

}

#endif