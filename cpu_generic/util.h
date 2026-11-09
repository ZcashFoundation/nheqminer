// Copyright (c) 2026 The Zcash Foundation
// Distributed under the MIT software license, see the accompanying
// file LICENSE_MIT or http://www.opensource.org/licenses/mit-license.php.

// The vendored `crypto/equihash.cpp` includes `../util.h` for `zcashd`'s
// logging. nheqminer has no such logger, and the solver's per-round tracing is
// not wanted in a miner's hot loop, so this supplies the symbol as a no-op.
// Keeping it here rather than editing the vendored file means
// `crypto/equihash.{h,cpp,tcc}` stay byte-identical to upstream; see README.md.

#pragma once

template <typename... Args>
static inline void LogPrint(const char *category, const char *format,
                            const Args &...) {
    (void)category;
    (void)format;
}
