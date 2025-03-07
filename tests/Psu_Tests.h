#pragma once	

#include <cryptoTools/Common/CLP.h>
#include <volePSI/config.h>

void Psu_setup_test(const oc::CLP& cmd);
void Psu_full_test(const oc::CLP& cmd);
void Psu_empty_test(const oc::CLP& cmd);
void Psu_partial_test(const oc::CLP& cmd);
void Psu_offline_test(const oc::CLP& cmd);