# cpu_generic

A CPU Equihash solver for arbitrary `(n, k)`, used to mine chains whose parameters are not
`(200, 9)` — Zcash Regtest uses `(48, 5)`, with 36-byte solutions.

The optimised solvers in this repository are specialised at compile time: `cpu_tromp` enumerates
`(200, 4)`, `(200, 8)` and `(144, 4)` and `#error`s on anything else, and its bucket layout has no
case for the 4 bucket bits `(48, 5)` implies. Rather than port that layout, this solver wraps the
generic implementation the mining pool's own share verifier already uses, so a solution this miner
produces is one the pool accepts by construction.

## Vendored files

`crypto/equihash.h`, `crypto/equihash.cpp` and `crypto/equihash.tcc` are copied **unmodified** from

    https://github.com/upbqdn/equihashverify
    commit bdd4a788924f2c51799664a09c54c0af2ac4ea10

which is the dependency `s-nomp` builds its `equihashverify` native module from. They descend from
`zcashd`'s `src/crypto/equihash.*` and are MIT licensed (Jack Grigg, The Zcash developers, The
LitecoinZ developers, The s-nomp developers); see the copyright headers in each file.

To keep them byte-identical to upstream, the two things they need from a `zcashd` tree are supplied
around them instead of by editing them:

- `../util.h`, for `zcashd`'s `LogPrint`, is provided by `util.h` here as a no-op.
- `config/bitcoin-config.h` is already behind `#if defined(HAVE_CONFIG_H)`, which is not defined.

`crypto/common.h`, `crypto/sha256.h` and `utilstrencodings.h` come from nheqminer's own
`nheqminer/` directory via the include path.

Refreshing the vendored copy is therefore a re-download at a new commit, with no patches to
re-apply.

## Usage

    nheqminer -n 48 -k 5 -l <pool>:<port> -u <address>.<worker> -t <threads>

`-n`/`-k` default to `(200, 9)`, where the specialised solvers are faster and remain the default.
The generic solver supports the parameter sets the verifier instantiates: `(200, 9)`, `(192, 7)`,
`(144, 5)`, `(125, 4)`, `(96, 5)`, `(96, 3)` and `(48, 5)`.
