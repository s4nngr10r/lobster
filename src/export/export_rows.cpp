#include "lobster/export.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "lobster/book.hpp"
#include "lobster/replay.hpp"

#ifdef LOBSTER_HAS_ARROW
#include <arrow/api.h>
#include <arrow/io/api.h>
#include <arrow/ipc/api.h>
#endif

namespace lobster {

namespace {

std::string csv_optional(const std::optional<std::int64_t>& value) {
  return value ? std::to_string(*value) : "";
}

std::string csv_optional(const std::optional<std::uint64_t>& value) {
  return value ? std::to_string(*value) : "";
}

std::string csv_optional(const std::optional<double>& value) {
  if (!value) {
    return "";
  }
  std::ostringstream out;
  out.setf(std::ios::fixed);
  out.precision(6);
  out << *value;
  return out.str();
}

}  // namespace

std::vector<TopOfBookFeatures> collect_top_of_book_rows(
    const std::filesystem::path& journal_path) {
  OrderBook book;
  ReplayEngine replay;
  std::vector<TopOfBookFeatures> rows;
  replay.replay(journal_path, [&](const BookEvent& event) {
    book.apply(event);
    if (event.kind != EventKind::book_level && event.kind != EventKind::snapshot_level) {
      return;
    }
    rows.push_back(compute_top_of_book_features(
        event, book.top_of_book(), book.bid_depth_levels(), book.ask_depth_levels(),
        book.bid_levels(), book.ask_levels()));
  });
  return rows;
}

std::string feature_csv_header() {
  return "ts_ns,seq,source_first_id,source_last_id,best_bid_price,best_bid_qty,best_ask_price,best_ask_qty,bid_depth_1,ask_depth_1,bid_depth_5,ask_depth_5,bid_depth_10,ask_depth_10,bid_levels,ask_levels,spread_ticks,mid_price,microprice,imbalance";
}

std::string feature_csv_row(const TopOfBookFeatures& row) {
  std::ostringstream out;
  out << row.ts_ns << ',' << row.seq << ',' << row.source_first_id << ','
      << row.source_last_id << ',' << csv_optional(row.best_bid_price) << ','
      << csv_optional(row.best_bid_qty) << ',' << csv_optional(row.best_ask_price) << ','
      << csv_optional(row.best_ask_qty) << ',' << csv_optional(row.bid_depth_1) << ','
      << csv_optional(row.ask_depth_1) << ',' << csv_optional(row.bid_depth_5) << ','
      << csv_optional(row.ask_depth_5) << ',' << csv_optional(row.bid_depth_10) << ','
      << csv_optional(row.ask_depth_10) << ',' << row.bid_levels << ',' << row.ask_levels
      << ',' << csv_optional(row.spread_ticks) << ',' << csv_optional(row.mid_price) << ','
      << csv_optional(row.microprice) << ',' << csv_optional(row.imbalance);
  return out.str();
}

void write_top_of_book_csv(const std::filesystem::path& output_path,
                           const std::vector<TopOfBookFeatures>& rows) {
  std::ofstream out(output_path, std::ios::trunc);
  if (!out) {
    throw std::runtime_error("failed to open CSV output");
  }
  out << feature_csv_header() << '\n';
  for (const auto& row : rows) {
    out << feature_csv_row(row) << '\n';
  }
}

void export_top_of_book_csv(const std::filesystem::path& journal_path,
                            const std::filesystem::path& output_path) {
  write_top_of_book_csv(output_path, collect_top_of_book_rows(journal_path));
}

bool arrow_export_available() {
#ifdef LOBSTER_HAS_ARROW
  return true;
#else
  return false;
#endif
}

void export_top_of_book_arrow(const std::filesystem::path& journal_path,
                              const std::filesystem::path& output_path) {
#ifdef LOBSTER_HAS_ARROW
  const auto rows = collect_top_of_book_rows(journal_path);

  arrow::UInt64Builder ts_ns_builder;
  arrow::UInt64Builder seq_builder;
  arrow::UInt64Builder source_first_id_builder;
  arrow::UInt64Builder source_last_id_builder;
  arrow::Int64Builder best_bid_price_builder;
  arrow::UInt64Builder best_bid_qty_builder;
  arrow::Int64Builder best_ask_price_builder;
  arrow::UInt64Builder best_ask_qty_builder;
  arrow::UInt64Builder bid_depth_1_builder;
  arrow::UInt64Builder ask_depth_1_builder;
  arrow::UInt64Builder bid_depth_5_builder;
  arrow::UInt64Builder ask_depth_5_builder;
  arrow::UInt64Builder bid_depth_10_builder;
  arrow::UInt64Builder ask_depth_10_builder;
  arrow::UInt64Builder bid_levels_builder;
  arrow::UInt64Builder ask_levels_builder;
  arrow::Int64Builder spread_ticks_builder;
  arrow::DoubleBuilder mid_price_builder;
  arrow::DoubleBuilder microprice_builder;
  arrow::DoubleBuilder imbalance_builder;

  auto append_int64 = [](arrow::Int64Builder& builder,
                         const std::optional<std::int64_t>& value) {
    return value ? builder.Append(*value) : builder.AppendNull();
  };
  auto append_uint64 = [](arrow::UInt64Builder& builder,
                          const std::optional<std::uint64_t>& value) {
    return value ? builder.Append(*value) : builder.AppendNull();
  };
  auto append_double = [](arrow::DoubleBuilder& builder,
                          const std::optional<double>& value) {
    return value ? builder.Append(*value) : builder.AppendNull();
  };
  auto require_ok = [](const arrow::Status& status, const char* what) {
    if (!status.ok()) {
      throw std::runtime_error(std::string(what) + ": " + status.ToString());
    }
  };

  for (const auto& row : rows) {
    require_ok(ts_ns_builder.Append(row.ts_ns), "failed to append ts_ns");
    require_ok(seq_builder.Append(row.seq), "failed to append seq");
    require_ok(source_first_id_builder.Append(row.source_first_id),
               "failed to append source_first_id");
    require_ok(source_last_id_builder.Append(row.source_last_id),
               "failed to append source_last_id");
    require_ok(append_int64(best_bid_price_builder, row.best_bid_price),
               "failed to append best_bid_price");
    require_ok(append_uint64(best_bid_qty_builder, row.best_bid_qty),
               "failed to append best_bid_qty");
    require_ok(append_int64(best_ask_price_builder, row.best_ask_price),
               "failed to append best_ask_price");
    require_ok(append_uint64(best_ask_qty_builder, row.best_ask_qty),
               "failed to append best_ask_qty");
    require_ok(append_uint64(bid_depth_1_builder, row.bid_depth_1),
               "failed to append bid_depth_1");
    require_ok(append_uint64(ask_depth_1_builder, row.ask_depth_1),
               "failed to append ask_depth_1");
    require_ok(append_uint64(bid_depth_5_builder, row.bid_depth_5),
               "failed to append bid_depth_5");
    require_ok(append_uint64(ask_depth_5_builder, row.ask_depth_5),
               "failed to append ask_depth_5");
    require_ok(append_uint64(bid_depth_10_builder, row.bid_depth_10),
               "failed to append bid_depth_10");
    require_ok(append_uint64(ask_depth_10_builder, row.ask_depth_10),
               "failed to append ask_depth_10");
    require_ok(bid_levels_builder.Append(row.bid_levels), "failed to append bid_levels");
    require_ok(ask_levels_builder.Append(row.ask_levels), "failed to append ask_levels");
    require_ok(append_int64(spread_ticks_builder, row.spread_ticks),
               "failed to append spread_ticks");
    require_ok(append_double(mid_price_builder, row.mid_price),
               "failed to append mid_price");
    require_ok(append_double(microprice_builder, row.microprice),
               "failed to append microprice");
    require_ok(append_double(imbalance_builder, row.imbalance),
               "failed to append imbalance");
  }

  std::shared_ptr<arrow::Array> ts_ns_array;
  std::shared_ptr<arrow::Array> seq_array;
  std::shared_ptr<arrow::Array> source_first_id_array;
  std::shared_ptr<arrow::Array> source_last_id_array;
  std::shared_ptr<arrow::Array> best_bid_price_array;
  std::shared_ptr<arrow::Array> best_bid_qty_array;
  std::shared_ptr<arrow::Array> best_ask_price_array;
  std::shared_ptr<arrow::Array> best_ask_qty_array;
  std::shared_ptr<arrow::Array> bid_depth_1_array;
  std::shared_ptr<arrow::Array> ask_depth_1_array;
  std::shared_ptr<arrow::Array> bid_depth_5_array;
  std::shared_ptr<arrow::Array> ask_depth_5_array;
  std::shared_ptr<arrow::Array> bid_depth_10_array;
  std::shared_ptr<arrow::Array> ask_depth_10_array;
  std::shared_ptr<arrow::Array> bid_levels_array;
  std::shared_ptr<arrow::Array> ask_levels_array;
  std::shared_ptr<arrow::Array> spread_ticks_array;
  std::shared_ptr<arrow::Array> mid_price_array;
  std::shared_ptr<arrow::Array> microprice_array;
  std::shared_ptr<arrow::Array> imbalance_array;

  require_ok(ts_ns_builder.Finish(&ts_ns_array), "failed to finish ts_ns");
  require_ok(seq_builder.Finish(&seq_array), "failed to finish seq");
  require_ok(source_first_id_builder.Finish(&source_first_id_array),
             "failed to finish source_first_id");
  require_ok(source_last_id_builder.Finish(&source_last_id_array),
             "failed to finish source_last_id");
  require_ok(best_bid_price_builder.Finish(&best_bid_price_array),
             "failed to finish best_bid_price");
  require_ok(best_bid_qty_builder.Finish(&best_bid_qty_array),
             "failed to finish best_bid_qty");
  require_ok(best_ask_price_builder.Finish(&best_ask_price_array),
             "failed to finish best_ask_price");
  require_ok(best_ask_qty_builder.Finish(&best_ask_qty_array),
             "failed to finish best_ask_qty");
  require_ok(bid_depth_1_builder.Finish(&bid_depth_1_array),
             "failed to finish bid_depth_1");
  require_ok(ask_depth_1_builder.Finish(&ask_depth_1_array),
             "failed to finish ask_depth_1");
  require_ok(bid_depth_5_builder.Finish(&bid_depth_5_array),
             "failed to finish bid_depth_5");
  require_ok(ask_depth_5_builder.Finish(&ask_depth_5_array),
             "failed to finish ask_depth_5");
  require_ok(bid_depth_10_builder.Finish(&bid_depth_10_array),
             "failed to finish bid_depth_10");
  require_ok(ask_depth_10_builder.Finish(&ask_depth_10_array),
             "failed to finish ask_depth_10");
  require_ok(bid_levels_builder.Finish(&bid_levels_array),
             "failed to finish bid_levels");
  require_ok(ask_levels_builder.Finish(&ask_levels_array),
             "failed to finish ask_levels");
  require_ok(spread_ticks_builder.Finish(&spread_ticks_array),
             "failed to finish spread_ticks");
  require_ok(mid_price_builder.Finish(&mid_price_array), "failed to finish mid_price");
  require_ok(microprice_builder.Finish(&microprice_array),
             "failed to finish microprice");
  require_ok(imbalance_builder.Finish(&imbalance_array),
             "failed to finish imbalance");

  auto schema = arrow::schema({
      arrow::field("ts_ns", arrow::uint64(), false),
      arrow::field("seq", arrow::uint64(), false),
      arrow::field("source_first_id", arrow::uint64(), false),
      arrow::field("source_last_id", arrow::uint64(), false),
      arrow::field("best_bid_price", arrow::int64()),
      arrow::field("best_bid_qty", arrow::uint64()),
      arrow::field("best_ask_price", arrow::int64()),
      arrow::field("best_ask_qty", arrow::uint64()),
      arrow::field("bid_depth_1", arrow::uint64()),
      arrow::field("ask_depth_1", arrow::uint64()),
      arrow::field("bid_depth_5", arrow::uint64()),
      arrow::field("ask_depth_5", arrow::uint64()),
      arrow::field("bid_depth_10", arrow::uint64()),
      arrow::field("ask_depth_10", arrow::uint64()),
      arrow::field("bid_levels", arrow::uint64(), false),
      arrow::field("ask_levels", arrow::uint64(), false),
      arrow::field("spread_ticks", arrow::int64()),
      arrow::field("mid_price", arrow::float64()),
      arrow::field("microprice", arrow::float64()),
      arrow::field("imbalance", arrow::float64()),
  });
  auto table = arrow::Table::Make(
      schema, {ts_ns_array, seq_array, source_first_id_array, source_last_id_array,
               best_bid_price_array, best_bid_qty_array, best_ask_price_array,
               best_ask_qty_array, bid_depth_1_array, ask_depth_1_array,
               bid_depth_5_array, ask_depth_5_array, bid_depth_10_array,
               ask_depth_10_array, bid_levels_array, ask_levels_array,
               spread_ticks_array,
               mid_price_array, microprice_array, imbalance_array});

  auto maybe_file = arrow::io::FileOutputStream::Open(output_path.string());
  if (!maybe_file.ok()) {
    throw std::runtime_error("failed to open Arrow output: " +
                             maybe_file.status().ToString());
  }
  auto output = *maybe_file;
  auto maybe_writer = arrow::ipc::MakeFileWriter(output.get(), table->schema());
  if (!maybe_writer.ok()) {
    throw std::runtime_error("failed to create Arrow writer: " +
                             maybe_writer.status().ToString());
  }
  auto writer = *maybe_writer;
  require_ok(writer->WriteTable(*table), "failed to write Arrow table");
  require_ok(writer->Close(), "failed to close Arrow writer");
  require_ok(output->Close(), "failed to close Arrow output");
#else
  (void)journal_path;
  (void)output_path;
  throw std::runtime_error("Arrow export is not available in this build");
#endif
}

}  // namespace lobster
