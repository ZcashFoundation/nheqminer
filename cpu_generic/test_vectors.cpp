// Copyright (c) 2026 The Zcash Foundation
// Distributed under the MIT software license, see the accompanying
// file LICENSE_MIT or http://www.opensource.org/licenses/mit-license.php.

// Tests the generic Equihash solver, in two ways:
//
// 1. A known-answer test over a fixture whose validity was established by an
// implementation that
//    shares no code with this one: `is_valid_solution()` from the Rust
//    `equihash` crate, the same one `zebra-chain` verifies blocks with. It
//    fails if the vendored verifier ever stops accepting a solution that is
//    known to be good, which no amount of solver self-consistency would catch.
//
// 2. A round trip: solve a fixed header, then verify what came back. This fails
// if the solver
//    stops producing valid solutions, independently of whether it still agrees
//    with itself.
//
//     cpu_generic_test [n] [k]
//
// Exits 0 only if both pass. Registered with CTest as cpu_generic_48_5.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "crypto/equihash.h"

namespace {

// The (48, 5) fixture. `cpu_generic` produced this solution for this header and
// nonce, and the Rust `equihash` crate confirmed it:
//
//     equihash::is_valid_solution(48, 5, input, nonce, solution) -> Ok(())
//
// Regenerate by running this test, which prints any solution it finds in the
// same format.
const char *FIXTURE_INPUT_HEX =
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
    "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
    "404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f"
    "606162636465666768696a6b";
const char *FIXTURE_NONCE_HEX =
    "07000000707172737475767778797a7b7c7d7e7f808182838485868788898a8b";
const char *FIXTURE_SOLUTION_HEX =
    "007c5cfce131d621aa2154ced9058d1989c722e74b8f19a50e9f8d25a2a45d04378f19a5";

std::vector<unsigned char> FromHex(const std::string &hex) {
    std::vector<unsigned char> bytes(hex.size() / 2);
    for (size_t i = 0; i < bytes.size(); i++) {
        bytes[i] = static_cast<unsigned char>(
            strtoul(hex.substr(i * 2, 2).c_str(), nullptr, 16));
    }
    return bytes;
}

std::string ToHex(const std::vector<unsigned char> &bytes) {
    std::string hex;
    hex.reserve(bytes.size() * 2);
    for (unsigned char byte : bytes) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", byte);
        hex += buf;
    }
    return hex;
}

// A fixed, arbitrary 140-byte header: 108 bytes of block header followed by a
// 32-byte nonce. `i % 251` keeps the bytes non-uniform and the vector easy to
// restate in another language.
std::vector<unsigned char> RoundTripHeader() {
    std::vector<unsigned char> header(140);
    for (size_t i = 0; i < header.size(); i++) {
        header[i] = static_cast<unsigned char>(i % 251);
    }
    return header;
}

eh_HashState StateFor(unsigned int n, unsigned int k,
                      const std::vector<unsigned char> &header) {
    eh_HashState state;
    EhInitialiseState(n, k, state, "ZcashPoW");
    crypto_generichash_blake2b_update(&state, header.data(), header.size());
    return state;
}

// 1. The fixture must still verify.
bool KnownAnswer(unsigned int n, unsigned int k) {
    if (n != 48 || k != 5) {
        printf("known answer: skipped, fixture is (48, 5)\n");
        return true;
    }

    std::vector<unsigned char> header = FromHex(FIXTURE_INPUT_HEX);
    const std::vector<unsigned char> nonce = FromHex(FIXTURE_NONCE_HEX);
    header.insert(header.end(), nonce.begin(), nonce.end());
    const std::vector<unsigned char> soln = FromHex(FIXTURE_SOLUTION_HEX);

    if (header.size() != 140 || soln.size() != equihash_solution_size(n, k)) {
        printf("known answer: FAIL, fixture is malformed (header %zu, solution "
               "%zu)\n",
               header.size(), soln.size());
        return false;
    }

    eh_HashState state = StateFor(n, k, header);
    bool valid = false;
    EhIsValidSolution(n, k, state, soln, valid);
    printf("known answer: %s (Rust equihash says this fixture is valid)\n",
           valid ? "pass" : "FAIL");
    return valid;
}

// 2. Solving must still produce something that verifies.
bool RoundTrip(unsigned int n, unsigned int k) {
    std::vector<unsigned char> header = RoundTripHeader();
    int found = 0;
    int verified = 0;

    std::function<bool(std::vector<unsigned char>)> validBlock =
        [&](std::vector<unsigned char> soln) {
            found++;
            eh_HashState state = StateFor(n, k, header);
            bool valid = false;
            EhIsValidSolution(n, k, state, soln, valid);
            if (valid) {
                verified++;
            }
            printf("round trip: nonce %02x%02x%02x%02x solution (%zu bytes) %s "
                   "-> %s\n",
                   header[108], header[109], header[110], header[111],
                   soln.size(), ToHex(soln).c_str(),
                   valid ? "valid" : "INVALID");
            return true;
        };
    std::function<bool(EhSolverCancelCheck)> cancelled =
        [](EhSolverCancelCheck) { return false; };

    // Most nonces yield no solution, so iterate like a miner does.
    const uint32_t max_nonces = 200;
    for (uint32_t counter = 0; counter < max_nonces && found == 0; counter++) {
        memcpy(&header[108], &counter, sizeof(counter));
        eh_HashState state = StateFor(n, k, header);
        try {
            EhOptimisedSolve(n, k, state, validBlock, cancelled);
        } catch (const std::exception &error) {
            printf("round trip: FAIL, solver threw: %s\n", error.what());
            return false;
        }
    }

    printf("round trip: %s (%d found, %d verified)\n",
           (found > 0 && found == verified) ? "pass" : "FAIL", found, verified);
    return found > 0 && found == verified;
}

} // namespace

int main(int argc, char **argv) {
    unsigned int n = argc > 1 ? strtoul(argv[1], nullptr, 10) : 48;
    unsigned int k = argc > 2 ? strtoul(argv[2], nullptr, 10) : 5;
    printf("params: n=%u k=%u, solution size %zu bytes\n", n, k,
           equihash_solution_size(n, k));

    bool known = KnownAnswer(n, k);
    bool round_trip = RoundTrip(n, k);
    return (known && round_trip) ? 0 : 1;
}
