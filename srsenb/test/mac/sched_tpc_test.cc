/**
 * Copyright 2013-2021 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include "srsenb/hdr/stack/mac/sched_ue_ctrl/tpc.h"
#include "srsran/common/test_common.h"

namespace srsenb {

int8_t decode_tpc(uint8_t encoded_tpc)
{
  const static int8_t tpc_table[] = {-1, 0, 1, 3};
  return encoded_tpc < sizeof(tpc_table) ? tpc_table[encoded_tpc] : std::numeric_limits<int8_t>::max();
}

int test_finite_target_snr()
{
  const uint32_t nof_prbs   = 50;
  const int      target_snr = 15;

  tpc tpcfsm(nof_prbs, 15, 15, true);

  // TEST: While no SNR info is provided, no TPC commands are sent
  for (uint32_t i = 0; i < 100; ++i) {
    tpcfsm.new_tti();
    TESTASSERT(decode_tpc(tpcfsm.encode_pucch_tpc()) == 0);
    TESTASSERT(decode_tpc(tpcfsm.encode_pusch_tpc()) == 0);
  }

  // TEST: current SNR above target SNR. Checks:
  // - TPC commands should be sent to decrease power
  // - The sum power of TPC commands should not exceed the difference between current and target SNRs
  int snr_diff = 10;
  tpcfsm.set_snr(target_snr + snr_diff, tpc::PUSCH_CODE);
  tpcfsm.set_snr(target_snr + snr_diff, tpc::PUCCH_CODE);
  int sum_pusch = 0, sum_pucch = 0;
  for (uint32_t i = 0; i < 100; ++i) {
    tpcfsm.new_tti();
    sum_pusch += decode_tpc(tpcfsm.encode_pusch_tpc());
    TESTASSERT(sum_pusch < 0 and sum_pusch >= -snr_diff);
    sum_pucch += decode_tpc(tpcfsm.encode_pucch_tpc());
    TESTASSERT(sum_pucch < 0 and sum_pucch >= -snr_diff);
  }

  // TEST: current SNR below target SNR. Checks:
  // - TPC commands should be sent to increase power
  // - The sum of TPC commands should not exceed the difference between current and target SNRs
  snr_diff = -10;
  tpcfsm.set_snr(target_snr + snr_diff, tpc::PUSCH_CODE);
  tpcfsm.set_snr(target_snr + snr_diff, tpc::PUCCH_CODE);
  sum_pusch = 0;
  sum_pucch = 0;
  for (uint32_t i = 0; i < 100; ++i) {
    tpcfsm.new_tti();
    sum_pusch += decode_tpc(tpcfsm.encode_pusch_tpc());
    TESTASSERT(sum_pusch > 0 and sum_pusch <= -snr_diff);
    sum_pucch += decode_tpc(tpcfsm.encode_pucch_tpc());
    TESTASSERT(sum_pucch > 0 and sum_pucch <= -snr_diff);
  }

  return SRSRAN_SUCCESS;
}

int test_undefined_target_snr()
{
  const uint32_t nof_prbs = 50;

  tpc tpcfsm(nof_prbs, -1, -1, true);
  TESTASSERT(tpcfsm.max_ul_prbs() == 50);

  // TEST: While the PHR is not updated, a limited number of TPC commands should be sent
  int sum_pusch = 0, sum_pucch = 0;
  for (uint32_t i = 0; i < 100; ++i) {
    tpcfsm.new_tti();
    sum_pusch += decode_tpc(tpcfsm.encode_pusch_tpc());
    sum_pucch += decode_tpc(tpcfsm.encode_pucch_tpc());
  }
  TESTASSERT(sum_pusch <= 3 and sum_pusch >= -1);
  TESTASSERT(sum_pucch <= 3 and sum_pucch >= -1);

  // TEST: SNR info should not affect TPC in undefined target SNR mode
  int snr_info = 10;
  tpcfsm.set_snr(snr_info, tpc::PUSCH_CODE);
  tpcfsm.set_snr(snr_info, tpc::PUCCH_CODE);
  sum_pusch = 0;
  sum_pucch = 0;
  for (uint32_t i = 0; i < 100; ++i) {
    tpcfsm.new_tti();
    sum_pusch += decode_tpc(tpcfsm.encode_pusch_tpc());
    sum_pucch += decode_tpc(tpcfsm.encode_pucch_tpc());
  }
  TESTASSERT(sum_pusch == 0);
  TESTASSERT(sum_pucch == 0);

  // TEST: Check that high PHR allows full utilization of available PRBs, TPC remains at zero (no target SINR)
  int phr = 30;
  tpcfsm.set_phr(phr);
  TESTASSERT(tpcfsm.max_ul_prbs() == 50);
  sum_pusch = 0;
  sum_pucch = 0;
  for (uint32_t i = 0; i < 100; ++i) {
    tpcfsm.new_tti();
    sum_pusch += decode_tpc(tpcfsm.encode_pusch_tpc());
    sum_pucch += decode_tpc(tpcfsm.encode_pucch_tpc());
  }
  TESTASSERT(sum_pusch == 0 and sum_pucch == 0);

  // TEST: PHR is too low to allow all PRBs to be allocated. This event should not affect TPC commands
  phr = 5;
  tpcfsm.set_phr(phr);
  TESTASSERT(tpcfsm.max_ul_prbs() < 50);
  for (uint32_t i = 0; i < 100; ++i) {
    tpcfsm.new_tti();
    TESTASSERT(decode_tpc(tpcfsm.encode_pusch_tpc()) == 0);
    TESTASSERT(decode_tpc(tpcfsm.encode_pucch_tpc()) == 0);
  }

  // TEST: PHR is negative. The TPC should slightly decrease Tx UL power until next PHR
  phr = -1;
  tpcfsm.set_phr(phr);
  TESTASSERT(tpcfsm.max_ul_prbs() == tpc::PHR_NEG_NOF_PRB);
  sum_pusch = 0;
  sum_pucch = 0;
  for (uint32_t i = 0; i < 100; ++i) {
    tpcfsm.new_tti();
    sum_pusch += decode_tpc(tpcfsm.encode_pusch_tpc());
    sum_pucch += decode_tpc(tpcfsm.encode_pucch_tpc());
  }
  TESTASSERT(sum_pusch == 0);
  TESTASSERT(sum_pucch == 0);

  return SRSRAN_SUCCESS;
}

} // namespace srsenb

int main()
{
  TESTASSERT(srsenb::test_finite_target_snr() == 0);
  TESTASSERT(srsenb::test_undefined_target_snr() == 0);
  printf("Success\n");
}
