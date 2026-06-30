# Export

`lobster-export` runs on the replayed canonical journal, not on live capture.

That keeps feature computation downstream of the validated sequencing boundary and avoids duplicating logic in the ingress path.

## CSV

Always available:

```bash
./lobster/build/lobster-export --csv <journal-path> <output.csv>
```

Current schema:

```text
ts_ns,seq,source_first_id,source_last_id,best_bid_price,best_bid_qty,best_ask_price,best_ask_qty,bid_depth_1,ask_depth_1,bid_depth_5,ask_depth_5,bid_depth_10,ask_depth_10,bid_levels,ask_levels,spread_ticks,mid_price,microprice,imbalance
```

Rows are emitted after each replayed event, using the resulting top-of-book state.

## Features

Current top-of-book features:

- `bid_depth_1`, `ask_depth_1`
- `bid_depth_5`, `ask_depth_5`
- `bid_depth_10`, `ask_depth_10`
- `bid_levels`, `ask_levels`
- `spread_ticks = best_ask_price - best_bid_price`
- `mid_price = (best_bid_price + best_ask_price) / 2`
- `microprice = (bid_price * ask_qty + ask_price * bid_qty) / (bid_qty + ask_qty)`
- `imbalance = (bid_qty - ask_qty) / (bid_qty + ask_qty)`

These are intentionally simple. The point is to prove a downstream consumer path, not to build a factor library.

## Arrow

Arrow is optional behind `LOBSTER_ENABLE_ARROW`.

When Apache Arrow is installed and found by `pkg-config`, use:

```bash
./lobster/build/lobster-export --arrow <journal-path> <output.arrow>
```

This writes an Arrow IPC file with the same schema as the CSV export.

If Apache Arrow is not found by `pkg-config`, the build still succeeds and CSV export remains available. In that case, `lobster-export --arrow ...` fails with a clear runtime error.

## Local Status

On this machine, Apache Arrow `24.0.0` is installed via Homebrew and Arrow export is enabled in the current build.
