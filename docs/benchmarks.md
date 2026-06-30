# Benchmarks

Date: `2026-06-30`

These numbers were collected on a `2019 MacBook Pro` with an `Intel Core i9-9980HK` and `16 GB RAM`.

They measure local replay and export work only. They do not claim end-to-end exchange latency, kernel/network tuning, or durability cost under forced sync policies.

## Environment

- OS: `macOS 15.7.2`
- Architecture: `x86_64`
- Compiler: `Apple clang 17.0.0`
- Build: `cmake -S lobster -B lobster/build && cmake --build lobster/build`

## Synthetic Replay

Benchmark binary:

```bash
./lobster/build/lobster-bench-replay <event_count>
```

Workload:

- fixed-width `book_level` records
- alternating bid/ask updates
- 64 price buckets
- single-thread replay into the current `std::map` book

Results:

```text
./lobster/build/lobster-bench-replay 100000
events=100000 elapsed_ns=26755070 per_event_ns=267.551 events_per_second=3.73761e+06

./lobster/build/lobster-bench-replay 1000000
events=1000000 elapsed_ns=246179207 per_event_ns=246.179 events_per_second=4.06208e+06
```

Takeaway:

- synthetic replay throughput is roughly `3.74M` to `4.06M` events/sec
- per-event replay cost is roughly `246ns` to `268ns`

## Synthetic Order Book Apply

Benchmark binary:

```bash
./lobster/build/lobster-bench-order-book <event_count>
```

Result:

```text
./lobster/build/lobster-bench-order-book 100000
events=100000 elapsed_ns=16598285 per_event_ns=165.983 events_per_second=6.02472e+06 bid_levels=64 ask_levels=64
```

Takeaway:

- pure book-apply throughput is about `6.02M` events/sec
- per-event apply cost is about `166ns`

## Synthetic Journal Write

Benchmark binary:

```bash
./lobster/build/lobster-bench-write <event_count>
```

Result:

```text
./lobster/build/lobster-bench-write 100000
events=100000 bytes=5600024 elapsed_ns=13841528 events_per_second=7.22464e+06 MiB_per_second=385.839
```

Takeaway:

- append-only journal write throughput is about `7.22M` events/sec on this machine
- binary write bandwidth is about `386 MiB/s`

## Synthetic Peak Memory

Benchmark binary:

```bash
./lobster/build/lobster-bench-memory <event_count>
```

Result:

```text
./lobster/build/lobster-bench-memory 100000
events=100000 maxrss_kb=6246400 bid_levels=512 ask_levels=512
```

Takeaway:

- this is a regression signal, not a polished headline number
- on macOS, `ru_maxrss` is OS-specific and should be compared mainly against later runs on the same host

## Real Journal Pass

Capture command used for the journal below:

```bash
perl -e '$pid=fork // die; if($pid==0){ exec @ARGV or die $! } sleep 5; kill q(INT), $pid; waitpid($pid,0); exit($? >> 8);' \
  ./lobster/build/lobster-capture \
  --binance-usdtm-btcusdt \
  /private/tmp/binance-bench.bin
```

Observed journal:

- file: `/private/tmp/binance-bench.bin`
- size: `538K`
- rows via `lobster-dump`: `13,768`

Replay timing:

```bash
TIMEFORMAT='%3R'; time ./lobster/build/lobster-replay /private/tmp/binance-bench.bin
```

```text
bid=5826350000000@69200000 ask=5826360000000@861600000
0.351
```

CSV export timing:

```bash
TIMEFORMAT='%3R'; time ./lobster/build/lobster-export --csv /private/tmp/binance-bench.bin /private/tmp/binance-bench.csv
```

```text
0.166
```

CSV output size:

- `/private/tmp/binance-bench.csv`: `1.7M`

Arrow export timing:

```bash
TIMEFORMAT='%3R'; time ./lobster/build/lobster-export --arrow /private/tmp/binance-bench.bin /private/tmp/binance-bench.arrow
```

```text
0.129
```

Arrow output size:

- `/private/tmp/binance-bench.arrow`: `1.1M`

Takeaway:

- replaying a real 5-second Binance journal completed in about `0.351s`
- CSV feature export completed in about `0.166s`
- Arrow feature export completed in about `0.129s`
- on this journal, Arrow export was faster and smaller than CSV

## What These Numbers Do Not Measure

- Binance bootstrap wall-clock time split by REST snapshot vs. WebSocket buffering
- network jitter or packet loss behavior
- `fsync` / flush-heavy journaling cost
- CPU pinning, busy-spin, cache-line, or NUMA tuning
- alternative book data structures

Those belong in a later pass if you want this repo to lean harder into low-latency systems profiling.
