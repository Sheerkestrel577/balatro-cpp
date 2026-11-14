# naneinf — scaffold v2 (spec-aligned)

Build & test: `./build.sh && ./run_tests && ./naneinf_debug`

Matches balatro-engine-spec.md:
- joker.hpp     — the two FROZEN virtuals (interact w/ isCopy, passiveModifiers), PassiveMods, registry
- events.hpp    — full interaction enum + fat EventData
- card.hpp      — facts only (facts-vs-interpretations invariant)
- bignum.hpp    — mantissa+exponent; win = e >= 1024 inequality, never isinf
- hand_eval.*   — 12 types, "contains" bitmask, scoring-set selection,
                  Four Fingers / Shortcut / Smeared / Wild / Stone / Splash aware
- balatro.*     — run state, broadcast, copy-depth guard, interpretation
                  helpers (isFace/suitMask/suitMatches/suitCoverPossible/chance),
                  destruction queue, full §4 pipeline, blind requirements
                  (ante>8 formula is a PLACEHOLDER — transcribe before deep endless)
- jokers/       — one file per joker, self-registering; 7 demos covering:
                  blind event, independent+RNG, per-card+suit helper, passive rule,
                  passive identity, retrigger query, copy joker
- tests.cpp     — 32 tests: BigNum, hand eval (4-kings-flush = test #1),
                  golden plasma pipeline scores, Blueprint copy & cycle safety,
                  Pareidolia-counts-destroyed-6-as-face, naneinf plumbing
