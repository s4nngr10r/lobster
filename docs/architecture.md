# Architecture

Pipeline:

`Binance snapshot + diff/trade streams -> sync validator + recovery -> canonical event -> binary journal -> deterministic replay -> order book`

## Components

- `transport`: owns the canonical record layout and encode/decode helpers
- `capture bootstrap`: validates the Binance snapshot/diff bootstrap contract, emits canonical book/trade/heartbeat/status events, and re-bootstrap on depth gaps
- `journal`: append-only writer plus sequential reader
- `replay`: validates sequencing and dispatches events in log order
- `book`: applies full `L2` price-level upserts/deletes and exposes top-of-book views for replay/export consumers

## Design choices

- Single process, single thread: easier to reason about correctness before pretending to do Aeron-scale distribution.
- Fixed-width records: trivial to mmap later, trivial to benchmark now.
- Price and quantity are integer ticks/lots: no floating-point ambiguity.
- Capture has a real venue seam: Binance USD-M BTCUSDT REST snapshot plus combined diff-depth / aggTrade stream with `U/u/pu` validation.
- Snapshot bootstrap is implemented. The book is not top-of-book only; only the current feature rows are.
- Replay is the truth boundary: if the journal is correct, every downstream consumer sees the same event stream.

## Deferred on purpose

- multicast/unicast transport plumbing
- archive/replay persistence beyond a local journal file

Those should be added only after the journal contract stops moving.
