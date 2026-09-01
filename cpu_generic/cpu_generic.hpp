// Copyright (c) 2026 The Zcash Foundation
// Distributed under the MIT software license, see the accompanying
// file LICENSE_MIT or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// A CPU solver for arbitrary Equihash parameters, wrapping the generic solver
// from the pool's own share verifier (see README.md).
//
// The optimised solvers (tromp, xenoncat, djezo) are specialised for (200, 9)
// at compile time and reject other parameters, so they cannot mine a chain that
// uses different ones. Zcash Regtest uses (48, 5), whose 36-byte solutions this
// solver produces.
//
// It is slower than the specialised solvers, so `-n`/`-k` default to (200, 9)
// and the specialised solvers remain the default for Mainnet and Testnet.
struct cpu_generic {
    std::string getdevinfo() { return ""; }

    static void start(cpu_generic &device_context);

    static void stop(cpu_generic &device_context);

    static void solve(const char *tequihash_header,
                      unsigned int tequihash_header_len, const char *nonce,
                      unsigned int nonce_len, std::function<bool()> cancelf,
                      std::function<void(const std::vector<uint32_t> &, size_t,
                                         const unsigned char *)>
                          solutionf,
                      std::function<void(void)> hashdonef,
                      cpu_generic &device_context);

    std::string getname() {
        return "CPU-GENERIC-" + std::to_string(n) + "-" + std::to_string(k);
    }

    // Equihash parameters. The generic solver supports the sets the verifier
    // instantiates: (200, 9), (192, 7), (144, 5), (125, 4), (96, 5), (96, 3),
    // and (48, 5).
    unsigned int n = 200;
    unsigned int k = 9;

    // The first 8 bytes of the BLAKE2b personalisation; the solver appends `n`
    // and `k`.
    std::string personalization = "ZcashPoW";
};
