# cpu_generic

A CPU Equihash solver for arbitrary `(n, k)`, used to mine chains whose parameters are not
`(200, 9)` — Zcash Regtest uses `(48, 5)`, with 36-byte solutions.

The optimised solvers in this repository are specialised at compile time: `cpu_tromp` enumerates
`(200, 4)`, `(200, 8)` and `(144, 4)` and `#error`s on anything else, and its bucket layout has no
case for the 4 bucket bits `(48, 5)` implies. Rather than port that layout, this solver wraps the
generic implementation the mining pool's own share verifier already uses, so a solution this miner
produces is one the pool accepts by construction.

## Vendored files

`crypto/equihash.h`, `crypto/equihash.cpp` and `crypto/equihash.tcc` are copied from

    https://github.com/upbqdn/equihashverify
    commit bdd4a788924f2c51799664a09c54c0af2ac4ea10

which is the dependency `s-nomp` builds its `equihashverify` native module from. They descend from
`zcashd`'s `src/crypto/equihash.*` and are MIT licensed (Jack Grigg, The Zcash developers, The
LitecoinZ developers, The s-nomp developers); see the copyright headers in each file.

`equihash.h` and `equihash.tcc` are byte-identical to upstream. `equihash.cpp` carries one
two-line fix, because upstream never compiles its mining path -- `equihashverify` builds only the
verifier, with `ENABLE_MINING` undefined, so `BasicSolve` has rotted:

```
-    GenerateHash(base_state, g, tmpHash, HashOutput, twist);
+    unsigned char tmpHash[HashOutput];
     for (eh_index g = 0; X.size() < init_size; g++) {
-        GenerateHash(base_state, g, tmpHash, HashOutput);
+        GenerateHash(base_state, g, tmpHash, HashOutput, twist);
```

It declared no `tmpHash`, and called `GenerateHash` once before the loop with a `g` that is not in
scope there and once inside it without the `twist` argument the signature requires. The fix matches
what the same file's own working `OptimisedSolve` does (`equihash.cpp:556-558`), and what upstream
`zcashd` does (`src/crypto/equihash.cpp:352-354` at v5.10.0). `Equihash::BasicSolve` is not called
from here -- only `EhOptimisedSolve` is -- but the translation unit has to compile.

The other two things these files want from a `zcashd` tree are supplied around them rather than by
editing them:

- `../util.h`, for `zcashd`'s `LogPrint`, is provided by `util.h` here as a no-op.
- `config/bitcoin-config.h` is already behind `#if defined(HAVE_CONFIG_H)`, which is not defined.

`crypto/common.h`, `crypto/sha256.h` and `utilstrencodings.h` come from nheqminer's own
`nheqminer/` directory via the include path.

Refreshing the vendored copy is a re-download at a new commit plus that one two-line fix, unless
upstream has repaired its mining path by then.

## Usage

    nheqminer -n 48 -k 5 -l <pool>:<port> -u <address>.<worker> -t <threads>

`-n`/`-k` default to `(200, 9)`, where the specialised solvers are faster and remain the default.
The generic solver supports the parameter sets the verifier instantiates: `(200, 9)`, `(192, 7)`,
`(144, 5)`, `(125, 4)`, `(96, 5)`, `(96, 3)` and `(48, 5)`.
