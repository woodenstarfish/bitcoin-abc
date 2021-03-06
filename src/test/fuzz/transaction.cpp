// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <coins.h>
#include <consensus/tx_check.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <core_memusage.h>
#include <policy/policy.h>
#include <policy/settings.h>
#include <primitives/transaction.h>
#include <streams.h>
#include <test/fuzz/fuzz.h>
#include <validation.h>
#include <version.h>

#include <cassert>

void test_one_input(const std::vector<uint8_t> &buffer) {
    CDataStream ds(buffer, SER_NETWORK, INIT_PROTO_VERSION);
    try {
        int nVersion;
        ds >> nVersion;
        ds.SetVersion(nVersion);
    } catch (const std::ios_base::failure &e) {
        return;
    }
    bool valid = true;
    const CTransaction tx = [&] {
        try {
            return CTransaction(deserialize, ds);
        } catch (const std::ios_base::failure &e) {
            valid = false;
            return CTransaction();
        }
    }();
    if (!valid) {
        return;
    }

    CValidationState state;
    (void)CheckRegularTransaction(tx, state);

    const CFeeRate dust_relay_fee{DUST_RELAY_TX_FEE};
    std::string reason;
    const bool is_standard_with_permit_bare_multisig = IsStandardTx(
        tx, /* permit_bare_multisig= */ true, dust_relay_fee, reason);
    const bool is_standard_without_permit_bare_multisig = IsStandardTx(
        tx, /* permit_bare_multisig= */ false, dust_relay_fee, reason);
    if (is_standard_without_permit_bare_multisig) {
        assert(is_standard_with_permit_bare_multisig);
    }
    std::unique_ptr<CChainParams> params =
        CreateChainParams(CBaseChainParams::REGTEST);
    (void)tx.GetHash();
    (void)tx.GetTotalSize();
    try {
        (void)tx.GetValueOut();
    } catch (const std::runtime_error &) {
    }
    (void)tx.IsCoinBase();
    (void)tx.IsNull();
    (void)tx.ToString();

    (void)EncodeHexTx(tx);
    (void)GetVirtualTransactionSize(tx);
    (void)ContextualCheckTransaction(params->GetConsensus(), tx, state, 1024,
                                     1024, 0);
    (void)IsStandardTx(tx, reason);
    (void)RecursiveDynamicUsage(tx);
}
