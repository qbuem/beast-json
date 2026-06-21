// test_jsontestsuite.cpp — conformance against the canonical JSONTestSuite corpus
// ("Parsing JSON is a Minefield", nst/JSONTestSuite), the de-facto adversarial
// test set for JSON parsers.
//
// The corpus is NOT vendored (it would bloat the repo and carries its own
// licence); CI clones it at a pinned commit and points us at it via the
// QBUEM_JSONTESTSUITE_DIR environment variable. When that variable is unset
// (the normal local/matrix build), every test here GTEST_SKIPs — so this file
// is a no-op unless the corpus is present.
//
// Naming convention (JSONTestSuite):
//   y_ → MUST be accepted (well-formed JSON)            → we assert parse_strict OK
//   n_ → MUST be rejected (malformed JSON)              → we assert parse_strict throws
//   i_ → implementation-defined (either is RFC-legal)   → we freeze our documented
//        profile below so a behavioural drift is caught and consciously reviewed.
//
// We test against parse_strict() (RFC 8259 + well-formed UTF-8), the mode whose
// whole contract is "reject anything not strictly valid".

#include <qbuem_json/qbuem_json.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace fs = std::filesystem;

// Path to the JSONTestSuite checkout's `test_parsing` directory, or empty if the
// corpus is not available (then every test skips).
fs::path corpus_dir() {
  const char *root = std::getenv("QBUEM_JSONTESTSUITE_DIR");
  if (!root || !*root) return {};
  fs::path tp = fs::path(root) / "test_parsing";
  std::error_code ec;
  if (!fs::is_directory(tp, ec)) return {};
  return tp;
}

std::string read_file(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

// True iff parse_strict accepts `bytes`. Any thrown type counts as a rejection —
// a conformant parser may signal malformed input however it likes.
bool strict_accepts(const std::string &bytes) {
  try {
    qbuem::Document doc;
    qbuem::parse_strict(doc, bytes);
    return true;
  } catch (...) {
    return false;
  }
}

std::vector<fs::path> cases_with_prefix(const fs::path &dir, std::string_view prefix) {
  std::vector<fs::path> out;
  for (const auto &e : fs::directory_iterator(dir)) {
    if (e.path().extension() != ".json") continue;
    if (e.path().filename().string().rfind(std::string(prefix), 0) == 0)
      out.push_back(e.path());
  }
  std::sort(out.begin(), out.end());
  return out;
}

// ── Frozen implementation-defined profile (JSONTestSuite, pinned commit) ──────
// Our documented choices on the RFC-implementation-defined i_ cases. Coherent
// theme: we are STRICT on UTF-8/UTF-16 byte validity (reject every malformed
// encoding) but LENIENT on \u-escape surrogate pairing and numeric overflow
// (huge magnitudes clamp to ±inf / 0 per IEEE 754), and we accept nesting up to
// the parser's depth cap (1024). If a code change flips one of these, the
// matching test fails on purpose — update the docs and this list deliberately.
const std::set<std::string> kImplDefinedAccept = {
    "i_number_double_huge_neg_exp.json",
    "i_number_huge_exp.json",
    "i_number_neg_int_huge_exp.json",
    "i_number_pos_double_huge_exp.json",
    "i_number_real_neg_overflow.json",
    "i_number_real_pos_overflow.json",
    "i_number_real_underflow.json",
    "i_number_too_big_neg_int.json",
    "i_number_too_big_pos_int.json",
    "i_number_very_big_negative_int.json",
    "i_object_key_lone_2nd_surrogate.json",
    "i_string_1st_surrogate_but_2nd_missing.json",
    "i_string_1st_valid_surrogate_2nd_invalid.json",
    "i_string_incomplete_surrogate_and_escape_valid.json",
    "i_string_incomplete_surrogate_pair.json",
    "i_string_incomplete_surrogates_escape_valid.json",
    "i_string_invalid_lonely_surrogate.json",
    "i_string_invalid_surrogate.json",
    "i_string_inverted_surrogates_U+1D11E.json",
    "i_string_lone_second_surrogate.json",
    "i_structure_500_nested_arrays.json",
};
const std::set<std::string> kImplDefinedReject = {
    "i_string_UTF-16LE_with_BOM.json",
    "i_string_UTF-8_invalid_sequence.json",
    "i_string_UTF8_surrogate_U+D800.json",
    "i_string_invalid_utf-8.json",
    "i_string_iso_latin_1.json",
    "i_string_lone_utf8_continuation_byte.json",
    "i_string_not_in_unicode_range.json",
    "i_string_overlong_sequence_2_bytes.json",
    "i_string_overlong_sequence_6_bytes.json",
    "i_string_overlong_sequence_6_bytes_null.json",
    "i_string_truncated-utf-8.json",
    "i_string_utf16BE_no_BOM.json",
    "i_string_utf16LE_no_BOM.json",
    "i_structure_UTF-8_BOM_empty_object.json",
};
} // namespace

// y_ : every well-formed document must be accepted. A failure here means we
// wrongly REJECT valid JSON — a hard conformance bug.
TEST(JSONTestSuite, MustAcceptAllValid) {
  fs::path dir = corpus_dir();
  if (dir.empty()) GTEST_SKIP() << "QBUEM_JSONTESTSUITE_DIR not set";
  const auto files = cases_with_prefix(dir, "y_");
  ASSERT_FALSE(files.empty()) << "no y_ cases found in " << dir;
  std::vector<std::string> wrongly_rejected;
  for (const auto &p : files)
    if (!strict_accepts(read_file(p)))
      wrongly_rejected.push_back(p.filename().string());
  EXPECT_TRUE(wrongly_rejected.empty())
      << wrongly_rejected.size() << " valid documents wrongly rejected:\n  "
      << [&] {
           std::string s;
           for (const auto &n : wrongly_rejected) s += n + "\n  ";
           return s;
         }();
}

// n_ : every malformed document must be rejected. A failure here means we
// wrongly ACCEPT invalid JSON — the dangerous direction for untrusted input.
TEST(JSONTestSuite, MustRejectAllInvalid) {
  fs::path dir = corpus_dir();
  if (dir.empty()) GTEST_SKIP() << "QBUEM_JSONTESTSUITE_DIR not set";
  const auto files = cases_with_prefix(dir, "n_");
  ASSERT_FALSE(files.empty()) << "no n_ cases found in " << dir;
  std::vector<std::string> wrongly_accepted;
  for (const auto &p : files)
    if (strict_accepts(read_file(p)))
      wrongly_accepted.push_back(p.filename().string());
  EXPECT_TRUE(wrongly_accepted.empty())
      << wrongly_accepted.size() << " invalid documents wrongly accepted:\n  "
      << [&] {
           std::string s;
           for (const auto &n : wrongly_accepted) s += n + "\n  ";
           return s;
         }();
}

// i_ : implementation-defined. Either outcome is RFC-legal, so we don't judge
// right/wrong — we lock our documented profile so a silent behavioural drift is
// surfaced for deliberate review, and force any brand-new i_ case to be
// classified rather than silently ignored.
TEST(JSONTestSuite, ImplementationDefinedProfileIsStable) {
  fs::path dir = corpus_dir();
  if (dir.empty()) GTEST_SKIP() << "QBUEM_JSONTESTSUITE_DIR not set";
  const auto files = cases_with_prefix(dir, "i_");
  ASSERT_FALSE(files.empty()) << "no i_ cases found in " << dir;
  for (const auto &p : files) {
    const std::string name = p.filename().string();
    const bool accepted = strict_accepts(read_file(p));
    const bool known_accept = kImplDefinedAccept.count(name) != 0;
    const bool known_reject = kImplDefinedReject.count(name) != 0;
    ASSERT_TRUE(known_accept || known_reject)
        << "unclassified new i_ case '" << name
        << "' — decide our behaviour and add it to the frozen profile in "
           "test_jsontestsuite.cpp + docs";
    if (known_accept)
      EXPECT_TRUE(accepted) << name << " flipped to REJECTED — implementation-"
                               "defined profile changed; update docs + list deliberately";
    else
      EXPECT_FALSE(accepted) << name << " flipped to ACCEPTED — implementation-"
                                "defined profile changed; update docs + list deliberately";
  }
}
