#include "unit_tests.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA_Tests/fss/dcf_test.h"
#include "RingOA_Tests/fss/dpf_test.h"
#include "RingOA_Tests/fss/prg_test.h"
#include "RingOA_Tests/obliv_fmi/obliv_fmi_test.h"
#include "RingOA_Tests/obliv_fmi/obliv_rank_test.h"
#include "RingOA_Tests/obliv_fmi/sot_fmi_test.h"
#include "RingOA_Tests/obliv_fmi/sot_rank_test.h"
#include "RingOA_Tests/obliv_range/obliv_range_test.h"
#include "RingOA_Tests/obliv_range/sot_range_test.h"
#include "RingOA_Tests/protocol/ddcf_test.h"
#include "RingOA_Tests/protocol/equality_test.h"
#include "RingOA_Tests/protocol/integer_comparison_test.h"
#include "RingOA_Tests/protocol/min3_test.h"
#include "RingOA_Tests/protocol/obliv_select_test.h"
#include "RingOA_Tests/protocol/ringoa_test.h"
#include "RingOA_Tests/protocol/shared_ot_test.h"
#include "RingOA_Tests/protocol/zt_test.h"
#include "RingOA_Tests/sharing/rep3_sharing_binary_test.h"
#include "RingOA_Tests/sharing/rep3_sharing_ring_test.h"
#include "RingOA_Tests/sharing/sharing_2p_binary_test.h"
#include "RingOA_Tests/sharing/sharing_2p_ring_test.h"
#include "RingOA_Tests/utils/file_io_test.h"
#include "RingOA_Tests/utils/network_test.h"
#include "RingOA_Tests/utils/timer_test.h"
#include "RingOA_Tests/utils/utils_test.h"
#include "RingOA_Tests/wm/wm_test.h"

namespace test_ringoa {

void RegisterUtilsTests(osuCrypto::TestCollection &t) {
    t.add("Utils_Test", Utils_Test);
    t.add("Timer_Test", Timer_Test);
    t.add("Network_TwoPartyManager_Test", Network_TwoPartyManager_Test);
    t.add("Network_ThreePartyManager_Test", Network_ThreePartyManager_Test);
    t.add("File_Io_Test", File_Io_Test);
}

void RegisterFssTests(osuCrypto::TestCollection &t) {
    t.add("Prg_Test", Prg_Test);
    t.add("Dpf_Params_Test", Dpf_Params_Test);
    t.add("Dpf_EvalAt_Test", Dpf_EvalAt_Test);
    t.add("Dpf_Fde_Test", Dpf_Fde_Test);
    t.add("Dpf_Fde_One_Test", Dpf_Fde_One_Test);
    t.add("Dcf_EvalAt_Test", Dcf_EvalAt_Test);
    t.add("Dcf_Fde_Test", Dcf_Fde_Test);
}

void RegisterSharingTests(osuCrypto::TestCollection &t) {
    t.add("Additive2P_Evaluate_Offline_Test", Additive2P_Evaluate_Offline_Test);
    t.add("Additive2P_BeaverTriples_Test", Additive2P_BeaverTriples_Test);
    t.add("Additive2P_EvaluateAdd_Online_Test", Additive2P_EvaluateAdd_Online_Test);
    t.add("Additive2P_EvaluateMult_Online_Test", Additive2P_EvaluateMult_Online_Test);
    t.add("Additive2P_EvaluateSelect_Online_Test", Additive2P_EvaluateSelect_Online_Test);
    t.add("Binary2P_EvaluateXor_Offline_Test", Binary2P_EvaluateXor_Offline_Test);
    t.add("Binary2P_EvaluateXor_Online_Test", Binary2P_EvaluateXor_Online_Test);
    t.add("Binary2P_BeaverTriples_Test", Binary2P_BeaverTriples_Test);
    t.add("Binary2P_EvaluateAnd_Online_Test", Binary2P_EvaluateAnd_Online_Test);
    t.add("Rep3_Offline_Test", Rep3_Offline_Test);
    t.add("Rep3_Open_Online_Test", Rep3_Open_Online_Test);
    t.add("Rep3_EvaluateAdd_Online_Test", Rep3_EvaluateAdd_Online_Test);
    t.add("Rep3_EvaluateMult_Online_Test", Rep3_EvaluateMult_Online_Test);
    t.add("Rep3_EvaluateInnerProduct_Online_Test", Rep3_EvaluateInnerProduct_Online_Test);
    t.add("Rep3Binary_Offline_Test", Rep3Binary_Offline_Test);
    t.add("Rep3Binary_Open_Online_Test", Rep3Binary_Open_Online_Test);
    t.add("Rep3Binary_EvaluateXor_Online_Test", Rep3Binary_EvaluateXor_Online_Test);
    t.add("Rep3Binary_EvaluateAnd_Online_Test", Rep3Binary_EvaluateAnd_Online_Test);
    t.add("Rep3Binary_EvaluateSelect_Online_Test", Rep3Binary_EvaluateSelect_Online_Test);
}

void RegisterProtocolTests(osuCrypto::TestCollection &t) {
    t.add("Ddcf_EvalAt_Test", Ddcf_EvalAt_Test);
    t.add("Ddcf_Fde_Test", Ddcf_Fde_Test);
    t.add("ZeroTest_Offline_Test", ZeroTest_Offline_Test);
    t.add("ZeroTest_Online_Test", ZeroTest_Online_Test);
    t.add("Equality_Offline_Test", Equality_Offline_Test);
    t.add("Equality_Online_Test", Equality_Online_Test);
    t.add("IntegerComparison_Offline_Test", IntegerComparison_Offline_Test);
    t.add("IntegerComparison_Online_Test", IntegerComparison_Online_Test);
    t.add("IntegerComparison_FullDomain_Offline_Test", IntegerComparison_FullDomain_Offline_Test);
    t.add("IntegerComparison_FullDomain_Online_Test", IntegerComparison_FullDomain_Online_Test);
    t.add("Min3_Offline_Test", Min3_Offline_Test);
    t.add("Min3_Online_Test", Min3_Online_Test);
    t.add("OblivSelect_DataGen_Test", OblivSelect_DataGen_Test);
    t.add("OblivSelect_Preprocess_Test", OblivSelect_Preprocess_Test);
    t.add("OblivSelect_SingleBitMask_Online_Test", OblivSelect_SingleBitMask_Online_Test);
    t.add("OblivSelect_ShiftedAdditive_Online_Test", OblivSelect_ShiftedAdditive_Online_Test);
    t.add("SharedOT_DataGen_Test", SharedOT_DataGen_Test);
    t.add("SharedOT_Preprocess_Test", SharedOT_Preprocess_Test);
    t.add("SharedOT_Online_Test", SharedOT_Online_Test);
    t.add("RingOa_DataGen_Test", RingOa_DataGen_Test);
    t.add("RingOa_Preprocess_Test", RingOa_Preprocess_Test);
    t.add("RingOa_Online_Test", RingOa_Online_Test);
    t.add("RingOa_Fsc_DataGen_Test", RingOa_Fsc_DataGen_Test);
    t.add("RingOa_Fsc_Preprocess_Test", RingOa_Fsc_Preprocess_Test);
    t.add("RingOa_Fsc_Online_Test", RingOa_Fsc_Online_Test);
}

void RegisterWmTests(osuCrypto::TestCollection &t) {
    t.add("WaveletMatrix_Access_Test", WaveletMatrix_Access_Test);
    t.add("WaveletMatrix_Quantile_Test", WaveletMatrix_Quantile_Test);
    t.add("WaveletMatrix_RangeFreqTest", WaveletMatrix_RangeFreqTest);
    t.add("WaveletMatrix_TopK_Test", WaveletMatrix_TopK_Test);
    t.add("WaveletMatrix_RankCF_Test", WaveletMatrix_RankCF_Test);
    t.add("FMIndex_Test", FMIndex_Test);
}

void RegisterOblivFMITests(osuCrypto::TestCollection &t) {
    t.add("SotRank_DataGen_Test", SotRank_DataGen_Test);
    t.add("SotRank_Preprocess_Test", SotRank_Preprocess_Test);
    t.add("SotRank_Online_Test", SotRank_Online_Test);
    t.add("SotFMI_DataGen_Test", SotFMI_DataGen_Test);
    t.add("SotFMI_Preprocess_Test", SotFMI_Preprocess_Test);
    t.add("SotFMI_Online_Test", SotFMI_Online_Test);
    t.add("OblivRank_DataGen_Test", OblivRank_DataGen_Test);
    t.add("OblivRank_Preprocess_Test", OblivRank_Preprocess_Test);
    t.add("OblivRank_Online_Test", OblivRank_Online_Test);
    t.add("OblivRank_Fsc_DataGen_Test", OblivRank_Fsc_DataGen_Test);
    t.add("OblivRank_Fsc_Preprocess_Test", OblivRank_Fsc_Preprocess_Test);
    t.add("OblivRank_Fsc_Online_Test", OblivRank_Fsc_Online_Test);
    t.add("OblivFMI_DataGen_Test", OblivFMI_DataGen_Test);
    t.add("OblivFMI_Preprocess_Test", OblivFMI_Preprocess_Test);
    t.add("OblivFMI_Online_Test", OblivFMI_Online_Test);
    t.add("OblivFMI_Fsc_DataGen_Test", OblivFMI_Fsc_DataGen_Test);
    t.add("OblivFMI_Fsc_Preprocess_Test", OblivFMI_Fsc_Preprocess_Test);
    t.add("OblivFMI_Fsc_Online_Test", OblivFMI_Fsc_Online_Test);
}

void RegisterOblivRangeTests(osuCrypto::TestCollection &t) {
    t.add("SotRange_DataGen_Test", SotRange_DataGen_Test);
    t.add("SotRange_Preprocess_Test", SotRange_Preprocess_Test);
    t.add("SotRange_Online_Test", SotRange_Online_Test);
    t.add("OblivRange_DataGen_Test", OblivRange_DataGen_Test);
    t.add("OblivRange_Preprocess_Test", OblivRange_Preprocess_Test);
    t.add("OblivRange_Online_Test", OblivRange_Online_Test);
    t.add("OblivRange_Fsc_DataGen_Test", OblivRange_Fsc_DataGen_Test);
    t.add("OblivRange_Fsc_Preprocess_Test", OblivRange_Fsc_Preprocess_Test);
    t.add("OblivRange_Fsc_Online_Test", OblivRange_Fsc_Online_Test);
}

osuCrypto::TestCollection Tests([](osuCrypto::TestCollection &t) {
    RegisterUtilsTests(t);
    RegisterFssTests(t);
    RegisterSharingTests(t);
    RegisterProtocolTests(t);
    RegisterWmTests(t);
    RegisterOblivFMITests(t);
    RegisterOblivRangeTests(t);
});

}    // namespace test_ringoa
