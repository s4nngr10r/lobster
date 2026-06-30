# Binance Capture

Date: `2026-06-30`

This note records the short end-to-end workflow for a live capture against Binance USD-M BTCUSDT using the current `lobster-capture` adapter.

## Commands

Build:

```bash
cmake -S lobster -B lobster/build
cmake --build lobster/build
```

Run a short live capture and stop it cleanly with `SIGINT` after 5 seconds:

```bash
perl -e '$pid=fork // die; if($pid==0){ exec @ARGV or die $! } sleep 5; kill q(INT), $pid; waitpid($pid,0); exit($? >> 8);' \
  ./lobster/build/lobster-capture \
  --binance-usdtm-btcusdt \
  /private/tmp/binance-live.bin
```

Inspect the resulting journal:

```bash
./lobster/build/lobster-dump /private/tmp/binance-live.bin | head -n 25
./lobster/build/lobster-replay /private/tmp/binance-live.bin
```

## Expected Shape

`lobster-dump` prints:

```text
seq,ts_ns,source_first_id,source_last_id,kind,side,price_ticks,qty
1,....,....,....,venue_status,none,0,0
2,....,....,....,snapshot,bid,...,...
3,....,....,....,snapshot,ask,...,...
...
```

## Notes

- Use `SIGINT` for short demo captures. A hard kill can leave a truncated tail record.
- The journal sequence is local and monotonic. It is assigned after the Binance snapshot/diff sync rules pass.
- `source_first_id` and `source_last_id` preserve the venue update-id range that produced each canonical event.
- Prices and quantities are stored as scaled integers with `1e8` precision.
- The adapter emits `snapshot_level`, `book_level`, `trade`, `heartbeat`, and `venue_status` records.
- Generated live-capture artifacts are intentionally not treated as stable repo fixtures. Create a fresh file when you want to inspect a real run.
