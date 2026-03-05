#include "utils.hpp"
#include <beast_json/beast_json.hpp>
#include <nlohmann/json.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <simdjson.h>
#include <yyjson.h>

int main(int argc, char **argv) {
  const char *filename = (argc > 1) ? argv[1] : "twitter.json";
  std::string json_content = bench::read_file(filename);

  if (json_content.empty())
    return 1;

  const size_t iterations = 200;
  bench::print_header("File I/O Benchmark — " + std::string(filename));
  std::cout << "Size: " << (json_content.size() / 1024.0) << " KB"
            << "  Iterations: " << iterations << "\n";
  bench::print_table_header();

  std::vector<bench::Result> results;

  // 1. beast::lazy
  {
    beast::json::lazy::DocumentView ctx;
    beast::json::lazy::parse_reuse(ctx, json_content); // warm-up
    bench::Timer pt, st;
    pt.start();
    for (size_t i = 0; i < iterations; ++i)
      beast::json::lazy::parse_reuse(ctx, json_content);
    double p_ns = pt.elapsed_ns() / iterations;

    auto doc = beast::json::lazy::parse_reuse(ctx, json_content);
    std::string dump_buf;
    st.start();
    for (size_t i = 0; i < iterations; ++i)
      doc.dump(dump_buf);
    double s_ns = st.elapsed_ns() / iterations;

    std::string v_out = doc.dump();
    bool ok = false;
    try {
      ok = (nlohmann::json::parse(json_content) == nlohmann::json::parse(v_out));
    } catch (...) {}
    results.push_back({"beast::lazy", p_ns, s_ns, ok});
  }

  // 2. nlohmann
  {
    bench::Timer pt, st;
    pt.start();
    for (size_t i = 0; i < iterations; ++i)
      nlohmann::json::parse(json_content);
    double p_ns = pt.elapsed_ns() / iterations;
    nlohmann::json j = nlohmann::json::parse(json_content);
    st.start();
    for (size_t i = 0; i < iterations; ++i)
      j.dump();
    double s_ns = st.elapsed_ns() / iterations;
    results.push_back({"nlohmann/json", p_ns, s_ns, true});
  }

  // 3. yyjson
  {
    bench::Timer pt, st;
    pt.start();
    for (size_t i = 0; i < iterations; ++i) {
      yyjson_doc *d = yyjson_read(json_content.c_str(), json_content.size(), 0);
      yyjson_doc_free(d);
    }
    double p_ns = pt.elapsed_ns() / iterations;
    yyjson_doc *d = yyjson_read(json_content.c_str(), json_content.size(), 0);
    st.start();
    for (size_t i = 0; i < iterations; ++i) {
      size_t l;
      char *s = yyjson_write(d, 0, &l);
      free(s);
    }
    double s_ns = st.elapsed_ns() / iterations;
    yyjson_doc_free(d);
    results.push_back({"yyjson", p_ns, s_ns, true});
  }

  for (const auto &r : results)
    r.print();
  return 0;
}
