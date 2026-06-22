// RFC conformance against OFFICIAL external suites (not vendored — CI fetches
// them at a pinned commit and points us at them; the tests skip when absent):
//   - RFC 9535 JSONPath: the jsonpath-compliance-test-suite cts.json, via
//     QBUEM_JSONPATH_CTS. qbuem::query must accept every valid selector with the
//     expected nodelist and reject every invalid one. match()/search() (I-Regexp)
//     are an intentional non-support and are excluded.
//   - RFC 8785 JCS: the cyberphone/json-canonicalization test vectors, via
//     QBUEM_JCS_DIR. qbuem::canonicalize must be byte-exact.
#include <gtest/gtest.h>

#include "qbuem_json/qbuem_json.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::string slurp(const std::string &p) {
  std::ifstream f(p, std::ios::binary);
  std::ostringstream s; s << f.rdbuf(); return s.str();
}
std::string canon(const std::string &j) {
  try { return qbuem::canonicalize(std::string_view(j)); } catch (...) { return "<bad>"; }
}
std::string nodelist_json(const std::vector<qbuem::Value> &nodes) {
  std::string out = "[";
  for (size_t i = 0; i < nodes.size(); ++i) { if (i) out += ','; out += nodes[i].dump(); }
  out += ']';
  return out;
}
} // namespace

TEST(RfcConformance, JsonPathCts_RFC9535) {
  const char *path = std::getenv("QBUEM_JSONPATH_CTS");
  if (!path || !*path) GTEST_SKIP() << "QBUEM_JSONPATH_CTS not set";
  std::string text = slurp(path);
  ASSERT_FALSE(text.empty()) << "empty cts.json at " << path;

  qbuem::Document doc;
  qbuem::Value root = qbuem::parse(doc, text);
  std::vector<std::string> failures;
  int judged = 0, regex_skipped = 0;

  for (const qbuem::Value &t : root["tests"].elements()) {
    std::string name = t["name"].is_string() ? std::string(t["name"].decoded()) : "?";
    std::string sel = t["selector"].is_string() ? std::string(t["selector"].decoded()) : "";
    if (sel.find("match(") != std::string::npos || sel.find("search(") != std::string::npos) {
      ++regex_skipped; continue;
    }
    ++judged;
    const bool invalid = t["invalid_selector"].is_valid() && t["invalid_selector"].as<bool>();
    if (invalid) {
      bool threw = false;
      try { qbuem::Document d; qbuem::Value r = qbuem::parse(d, "{}"); (void)qbuem::query(r, sel); }
      catch (...) { threw = true; }
      if (!threw) failures.push_back("accepted-invalid: " + name + "  " + sel);
      continue;
    }
    std::vector<qbuem::Value> got;
    std::string docjson = t["document"].dump();
    try { qbuem::Document d; qbuem::Value r = qbuem::parse(d, std::string_view(docjson)); got = qbuem::query(r, sel); }
    catch (const std::exception &e) { failures.push_back("threw-on-valid: " + name + "  " + sel + "  " + e.what()); continue; }
    std::string gotc = canon(nodelist_json(got));
    bool ok = false;
    if (t["result"].is_valid()) ok = (gotc == canon(t["result"].dump()));
    else if (t["results"].is_valid()) { for (const auto &c : t["results"].elements()) if (gotc == canon(c.dump())) { ok = true; break; } }
    else ok = got.empty();
    if (!ok) failures.push_back("mismatch: " + name + "  " + sel);
  }
  std::string detail;
  for (size_t i = 0; i < failures.size() && i < 30; ++i) detail += "\n  " + failures[i];
  EXPECT_TRUE(failures.empty())
      << failures.size() << " of " << judged << " judged CTS tests failed ("
      << regex_skipped << " match/search excluded):" << detail;
}

TEST(RfcConformance, JcsVectors_RFC8785) {
  const char *dir = std::getenv("QBUEM_JCS_DIR"); // .../testdata
  if (!dir || !*dir) GTEST_SKIP() << "QBUEM_JCS_DIR not set";
  const char *names[] = {"arrays", "french", "structures", "unicode", "values", "weird"};
  for (const char *n : names) {
    std::string in = slurp(std::string(dir) + "/input/" + n + ".json");
    std::string exp = slurp(std::string(dir) + "/output/" + n + ".json");
    while (!exp.empty() && (exp.back() == '\n' || exp.back() == '\r')) exp.pop_back();
    ASSERT_FALSE(in.empty()) << "missing JCS input " << n;
    std::string got;
    ASSERT_NO_THROW(got = qbuem::canonicalize(std::string_view(in))) << n;
    EXPECT_EQ(got, exp) << "JCS vector '" << n << "' not byte-exact";
  }
}
