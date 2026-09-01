// Copyright (c) 2026 The Zcash Foundation
// Distributed under the MIT software license, see the accompanying
// file LICENSE_MIT or http://www.opensource.org/licenses/mit-license.php.

#include <functional>
#include <stdexcept>
#include <vector>

#include "cpu_generic.hpp"
#include "crypto/equihash.h"

void cpu_generic::start(cpu_generic &device_context) {}

void cpu_generic::stop(cpu_generic &device_context) {}

void cpu_generic::solve(const char *tequihash_header,
                        unsigned int tequihash_header_len, const char *nonce,
                        unsigned int nonce_len, std::function<bool()> cancelf,
                        std::function<void(const std::vector<uint32_t> &,
                                           size_t, const unsigned char *)>
                            solutionf,
                        std::function<void(void)> hashdonef,
                        cpu_generic &device_context) {
    // # Correctness
    //
    // This is the state construction the pool's share verifier uses
    // (`equihashverify.cc`: `EhInitialiseState(N, K, state, personalization)`
    // then one update over the 140-byte header). Feeding the same bytes through
    // the same solver means a solution this miner finds is a solution the pool
    // verifies, without either side reimplementing the other's assumptions.
    //
    // The stratum protocol splits the header: `tequihash_header` is its first
    // 108 bytes and `nonce` is the trailing 32, so two updates cover the same
    // 140 bytes.
    eh_HashState state;
    EhInitialiseState(device_context.n, device_context.k, state,
                      device_context.personalization.c_str());
    crypto_generichash_blake2b_update(
        &state, reinterpret_cast<const unsigned char *>(tequihash_header),
        tequihash_header_len);
    crypto_generichash_blake2b_update(
        &state, reinterpret_cast<const unsigned char *>(nonce), nonce_len);

    // `soln` is already in the minimal encoding that goes into a block header:
    // 36 bytes for (48, 5), 1344 for (200, 9). Passing it as `compressed_sol`
    // lets the caller use it directly instead of re-compressing indices, so the
    // byte count is the solution's real length.
    std::function<bool(std::vector<unsigned char>)> validBlock =
        [&solutionf](std::vector<unsigned char> soln) {
            solutionf(std::vector<uint32_t>(), soln.size(), soln.data());
            // Keep searching: this nonce can yield more than one solution, and
            // the caller decides which ones meet the target.
            return false;
        };

    std::function<bool(EhSolverCancelCheck)> cancelled =
        [&cancelf](EhSolverCancelCheck) { return cancelf(); };

    try {
        EhOptimisedSolve(device_context.n, device_context.k, state, validBlock,
                         cancelled);
    } catch (const EhSolverCancelledException &) {
        // The caller asked us to stop, usually because a new job arrived.
        return;
    }

    hashdonef();
}
