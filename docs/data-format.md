# Data Format

The journal starts with a fixed file header:

- magic: `LBJR`
- version: `1`
- header bytes: size of the header struct

The journal ends with a fixed footer:

- magic: `LBFT`
- footer bytes: size of the footer struct
- record count: number of fixed-width records in the file

Each event record is fixed width and little-endian:

- `ts_ns` `uint64_t`: capture timestamp in nanoseconds
- `seq` `uint64_t`: venue or canonical sequence number
- `source_first_id` `uint64_t`: source-side first update/trade id carried into the canonical record
- `source_last_id` `uint64_t`: source-side last update/trade id carried into the canonical record
- `price_ticks` `int64_t`: integer price
- `qty` `uint64_t`: resting quantity at that level, `0` means delete
- `side` `uint8_t`: `1 = bid`, `2 = ask`
- `kind` `uint8_t`: `1 = live book level`, `2 = snapshot level`, `3 = trade`, `4 = heartbeat`, `5 = venue_status`
- reserved padding for future flags/versioning

The first version keeps the schema intentionally narrow:

- one symbol
- one venue
- multiple canonical record kinds
- explicit source update ranges in the wire format

Snapshot levels may share the same sequence number in a venue-native model, but the live Binance adapter assigns a local canonical sequence per emitted record before journaling. Replay therefore sees a strictly increasing local sequence for all live-capture records.
