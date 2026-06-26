# Benchmarks

> Status: methodology defined; measured numbers are **post-v1** (the roadmap
> deliberately leads with a qualitative "unified + agent-native + closed-loop +
> full-lifecycle" narrative rather than an "Nx faster" headline). This page is
> the harness + the slots to fill once we cut a v1.0 benchmark tag.

## What we measure

1. **Sequential write throughput** — records/s and MB/s of an append-only write
   txn (small records + one large BIN payload), fsync per commit.
2. **Sequential scan** — records/s over the live set at a snapshot (the hot read
   path for transcoders).
3. **Point get by id** — µs/get after the index is warm.
4. **Round-trip fidelity + cost** — `.flux` → USD/OKF/A2A/MCAP → `.flux`, assert
   zero field loss; report encode/decode time.
5. **`.fluxa` ↔ `.flux` conv** — equivalence + time.
6. **Footprint** — `.flux` file size vs. raw payload; reference C lib code/ROM
   size (the MCU / Tier-2 budget).

## Harness

- Generator: synthetic record sets keyed by layer/kind/payload-size
  (`tests/bench.c`, to be added).
- Compare against: raw byte stream (lower bound), and (desktop-only, optional)
  an OpenUSD-backed equivalent for the USD projection.
- All numbers: platform, compiler, build type, and N must be stated; report
  median of ≥5 runs.

## Slots (to fill)

| metric | value | platform | date |
|---|---|---|---|
| seq write (rec/s) | — | — | — |
| scan (rec/s) | — | — | — |
| get (µs) | — | — | — |
| `.flux` size ratio | — | — | — |

## Fairness note

FLUXmeme is not a single-purpose format, so a raw "vs parquet/csv" race is
apples-to-oranges. The honest framing: **per-feature** cost (a unified asset
that projects to USD/OKF/A2A/MCAP) at near-raw-log read/write speed, with
bounded-footprint C and MCU portability as the differentiator rather than peak
throughput.
