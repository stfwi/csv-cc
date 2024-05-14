/**
 * @test compose-fixed
 *
 * Checks csv::csv_parser against a known
 * set of data (no fuzz).
 */
#include <testenv.hh>
#include <include/csv.hh>
#include <unordered_set>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <chrono>

void test_quoting_fixed()
{
  using namespace csv;
  using namespace std;
  test_info("Checking `csv_composer::quote()` ...");
  test_expect_eq(csv_composer::quote(""), "\"\"");
  test_expect_eq(csv_composer::quote("a"), "\"a\"");
  test_expect_eq(csv_composer::quote("'a'"), "\"'a'\"");
  test_expect_eq(csv_composer::quote(","), "\",\"");
  test_expect_eq(csv_composer::quote("\n"), "\"\n\"");
  test_expect_eq(csv_composer::quote("\r"), "\"\r\"");
  test_expect_eq(csv_composer::quote("\r\n"), "\"\r\n\"");
  test_expect_eq(csv_composer::quote("\""), "\"\"\"\"");

  test_info("NUL means EOT, stop parsing:");
  test_expect_ne(csv_composer::quote("\0"), "\"\0\"");
}

void test_escaping_fixed()
{
  using namespace csv;
  using namespace std;
  test_info("Checking `csv_composer::escape()` ...");
  const auto delim = ',';
  const auto composer = csv_composer(csv_composer::no_output, delim);
  test_expect_eq(composer.escape(""), "");
  test_expect_eq(composer.escape("a"), "a");
  test_expect_eq(composer.escape("'a'"), "'a'");
  test_expect_eq(composer.escape(string(1, delim)), string("\"") + delim + "\"");
  test_expect_eq(composer.escape("\n"), "\"\n\"");
  test_expect_eq(composer.escape("\r"), "\"\r\"");
  test_expect_eq(composer.escape("\r\n"), "\"\r\n\"");
  test_expect_eq(composer.escape("\""), "\"\"\"\"");
  test_info("NUL means EOT, stop parsing:");
  test_expect_ne(composer.escape("\0"), "\"\0\"");
}

void test_env_makerandom_file()
{
  using namespace std;
  const auto file_size_kb = size_t(1024);
  const auto n_cols = size_t(10);
  const auto delim = char(',');
  const auto csv_header = string();
  const auto max_field_len = size_t(32);
  const auto rnd_pool = string_view("123456789");
  const auto file_path =
    te::make_random_csv_file("testcsv", file_size_kb, n_cols, delim, csv_header, max_field_len, rnd_pool);
  test_note("CSV file generated is: " << file_path);

  auto chars_found = unordered_set<char>();
  // Read entire file, register all chars
  {
    auto fis = ifstream(file_path, ios::binary);
    std::for_each(
      std::istream_iterator<char>(fis), std::istream_iterator<char>(), [&](const auto& c) { chars_found.emplace(c); });
  }

  auto found_chars = string();
  auto invalid_chars = string();
  for(const auto c: chars_found) {
    if(c == '\n' || c == '\r' || c == delim || c == '"') continue;
    found_chars.push_back(c);
    if(rnd_pool.find(c) != rnd_pool.npos) continue;
    invalid_chars.push_back(c);
  }

  test_info("Found characters: '", found_chars, "'");
  test_info("Invalid characters: '", invalid_chars, "'");
  test_expect(invalid_chars.empty());
}

void test_compose_fixed()
{
  using namespace std;
  using namespace csv;

  {
    auto out = stringstream();
    const auto out_fn = [&](const string& line) { out << line; };
    auto composer = csv_composer(out_fn, ',', "\n");
    test_expect_eq(composer.delimiter(), ',');
    test_expect_eq(composer.newline(), "\n");
    test_expect_noexcept(composer.define_columns(5, array{1, 2}));
    test_expect_noexcept(composer.feed(array{"col1", "col2", "col3", "col4", "col5"}));
    test_expect_noexcept(composer.feed(array{"1", "2", "3", "4", "5"}));
    test_expect_except(composer.feed(array{"1", "2", "3", "4"}));
    test_expect_except(composer.feed(array{"1", "2", "3", "4", "5", "6"}));
    test_expect_noexcept(composer.feed(vector{"", "", "", "", "5"}));
    test_note("CSV lines:" << out.str());
    test_expect_noexcept(composer.clear());
    test_expect_noexcept(composer.define_columns(1));
    test_expect_noexcept(composer.clear());
    test_expect_except(composer.define_columns(0));
    test_expect_noexcept(composer.clear());
    test_expect_except(composer.define_columns(2, array{0}));
    test_expect_noexcept(composer.clear());
    test_expect_noexcept(composer.define_columns(2, array{1}));
    test_expect_noexcept(composer.clear());
    test_expect_noexcept(composer.define_columns(2, array{2}));
    test_expect_noexcept(composer.clear());
    test_expect_except(composer.define_columns(2, array{3}));
    test_expect_noexcept(composer.clear());
    test_expect_except(composer.define_columns(2, array{-1}));
  }

  {
    auto out = stringstream();
    const auto out_fn = [&](const string& line) { out << line; };
    auto composer = csv_composer(out_fn, ';', "\r\n");
    test_expect_eq(composer.delimiter(), ';');
    test_expect_eq(composer.newline(), "\r\n");
    test_expect_noexcept(composer.define_columns(5, array{1, 2}));
    test_expect_noexcept(composer.feed(array{"col1", "col2", "col3 ", " col4", ";col5"}));
    test_expect_noexcept(composer.feed(array{"1", "2", "3", "4", "5"}));
    test_expect_noexcept(composer.feed(vector{"", "", "\r", "\"", "\t5"}));
    test_note("CSV lines:" << out.str());
  }

  {
    auto out = stringstream();
    const auto out_fn = [&](const string& line) { out << line; };
    auto composer = csv_composer(out_fn, '\t', "\r\n");
    test_expect_eq(composer.delimiter(), '\t');
    test_expect_eq(composer.newline(), "\r\n");
    test_expect_noexcept(composer.define_columns(5, array{1, 2}));
    test_expect_noexcept(composer.feed(array{"col1", "col2", "col3", "col4", "col5"}));
    test_expect_noexcept(composer.feed(array{"1", "2", "3", "4", "5"}));
    test_expect_except(composer.feed(array{"1", "2", "3", "4"}));
    test_expect_except(composer.feed(array{"1", "2", "3", "4", "5", "6"}));
    test_expect_noexcept(composer.feed(vector{"", "", "\n", "\r\n", "5\t"}));
    test_note("CSV lines:" << out.str());
  }
}

void test(const std::vector<std::string>&)
{
  test_escaping_fixed();
  test_quoting_fixed();
  test_env_makerandom_file();
  test_compose_fixed();
}
