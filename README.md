# lobster

`lobster` is a compact `C++20` market-data transport, binary journal, and deterministic replay hobby project.

Current scope:

- fixed-width canonical journal records with source update ranges
- append-only binary journal with explicit versioning
- real Binance USD-M BTCUSDT snapshot + diff-depth bootstrap before journaling live flow
- trade, heartbeat, and venue-status records alongside book updates
- deterministic single-thread replay with sequence validation
- in-memory `L2` price-level book reconstruction
- replay/export path with depth-at-`N` top-of-book features
- replay, order-book, write-throughput, and peak-memory benchmarks

The hot path is deliberately boring: parse once, write fixed records, replay sequentially, rebuild the book without framework baggage.

## Why It Exists

The project is intentionally scoped around the systems pieces that were most interesting to build:

- canonical event layout
- strict bootstrap and gap handling
- deterministic replay
- full `L2` price-level rebuild
- simple downstream feature export
- measurable throughput on a laptop

It is not trying to be a full exchange simulator or an Aeron clone.

## Build

```bash
cmake -S lobster -B lobster/build
cmake --build lobster/build
ctest --test-dir lobster/build
```

## Binaries

- `lobster-capture`: live Binance USD-M BTCUSDT capture or legacy stdin fixture capture into the binary journal
- `lobster-dump`: prints journal records as CSV for inspection/debugging
- `lobster-export`: replays a journal into top-of-book feature rows as CSV or Arrow
- `lobster-replay`: replays a journal and prints final top-of-book state
- `lobster-bench-replay`: generates synthetic updates and reports replay throughput
- `lobster-bench-order-book`: measures pure book-apply throughput
- `lobster-bench-write`: measures journal write throughput
- `lobster-bench-memory`: reports process peak RSS after replaying a synthetic journal

## Live Capture

```bash
./lobster/build/lobster-capture --binance-usdtm-btcusdt lobster/data/sample/live.bin
```

This mode uses the official Binance USD-M futures contract documented for:

- REST snapshot: `GET /fapi/v1/depth?symbol=BTCUSDT&limit=1000`
- combined stream: `wss://fstream.binance.com/public/stream?streams=btcusdt@depth@100ms/btcusdt@aggTrade`

The adapter validates the documented bootstrap rules:

- buffer WebSocket diff events first
- fetch snapshot
- first processed diff must satisfy `U <= lastUpdateId <= u`
- every later diff must satisfy `pu == previous u`

The journal stores canonical per-level events after sync, with a local monotonic sequence assigned at capture time. It also records Binance source update ranges and emits `trade`, `heartbeat`, and `venue_status` records.

Prices and quantities are scaled fixed-point integers with `1e8` precision.

A concrete live-capture workflow is documented in [docs/binance-capture.md](docs/binance-capture.md).

## Legacy Fixture Capture

Each input line for `lobster-capture` is one of:

```text
SNAP <ts_ns> <snapshot_seq> <side> <price_ticks> <qty>
UPD <ts_ns> <seq> <side> <price_ticks> <qty>
SYNC <snapshot_seq>
```

Example:

```text
SNAP 100 10 B 10000 5
SNAP 100 10 A 10020 4
UPD 90 9 B 9990 2
UPD 110 11 B 10000 7
UPD 120 12 A 10020 0
SYNC 10
```

`qty == 0` deletes the level.

Before `SYNC`, incrementals are buffered. On `SYNC`, `lobster-capture` writes the snapshot, drops stale buffered updates with `seq <= snapshot_seq`, and requires the remaining incrementals to continue contiguously from `snapshot_seq + 1`.

## Journal Inspection

```bash
./lobster/build/lobster-capture lobster/data/sample/sample.bin < lobster/data/sample/capture.txt
./lobster/build/lobster-dump lobster/data/sample/sample.bin
```

Example output:

```text
seq,ts_ns,source_first_id,source_last_id,kind,side,price_ticks,qty
10,100,0,0,snapshot,bid,10000,5
10,100,0,0,snapshot,ask,10020,4
11,110,0,0,live,bid,10000,7
12,120,0,0,live,ask,10020,0
```

## Feature Export

CSV export is always available:

```bash
./lobster/build/lobster-export --csv lobster/data/sample/sample.bin /private/tmp/lobster-features.csv
./lobster/build/lobster-export --arrow lobster/data/sample/sample.bin /private/tmp/lobster-features.arrow
```

This writes one replay-time row per journal event with:

- best bid / best ask
- bid/ask depth at `1`, `5`, and `10` levels
- current bid/ask level counts
- spread
- mid price
- microprice
- top-of-book imbalance

The replayed book is full `L2` by price level. The current feature/export layer is still derived from top-of-book plus depth-at-`N`, not full-book tensors or deeper factor families.

Arrow export is optional and only builds when Apache Arrow is installed locally. If `pkg-config --modversion arrow` succeeds on your machine, `--arrow` is available.

## Why This Shape

This version is built around the parts that seemed most fun to prototype:

- canonical wire discipline
- deterministic replay
- explicit failure semantics on gaps/out-of-order data
- low-allocation book rebuild
- benchmarkable code paths

See [docs/architecture.md](docs/architecture.md) and [docs/data-format.md](docs/data-format.md).
