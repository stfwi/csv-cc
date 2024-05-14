/**
 * @test parse-fixed
 *
 * Test the CSV parser with a defined set of
 * csv data files and their corresponding expected
 * result files (which are the csv file names with
 * an additional ".txt" extension).
 */
#include <testenv.hh>
#include <include/csv.hh>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <chrono>

void test_parse_cmpfile(
  const std::filesystem::path path,
  const char delim,
  const std::string_view comment_chars,
  const std::string_view trim_chars)
{
  test_info("Checking against ", path);
  test_note(
    "Delimiter: '" << delim << "', header-comment-chars: '" << comment_chars << "', trim-chars: '" << trim_chars
                   << "'");

  auto parsed_lines = std::stringstream();

  const auto row_proc = [&](const std::vector<std::string>& fields, size_t line_no) {
    parsed_lines << te::csv_escape_joined_row_fields(fields, line_no) << '\n';
  };

  const auto trimmed = [](std::string&& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](char ch) { return !std::isspace(ch); }).base(), s.end());
    return s;
  };

  const auto file_contents = [&](const std::string& path) {
    auto fis = std::ifstream(path, std::ios::binary);
    return trimmed(std::string((std::istreambuf_iterator<char>(fis)), std::istreambuf_iterator<char>()));
  };

  test_expect_noexcept(csv::csv_parser(row_proc, delim, comment_chars, trim_chars).parse_file(path));
  test_note("-- file:" << path << "\n" << parsed_lines.str());

  const auto check_file = path.string() + ".txt";
  const auto check_file_contents = file_contents(check_file);
  test_expect(!check_file_contents.empty());
  if(!test_expect_cond(trimmed(parsed_lines.str()) == check_file_contents)) {
    test_note("-- checkfile: " << check_file << "\n" << check_file_contents);
  }
}

void test_parse_cmpfile_all(
  const std::filesystem::path dir,
  const char delim,
  const std::string_view comment_chars,
  const std::string_view trim_chars)
{
  using namespace std;
  auto ls = filesystem::directory_iterator(dir);
  for(auto& e: ls) {
    if(!e.is_regular_file()) continue;
    test_note("Checking " << e.path() << "...");
    if(e.path().extension() != ".csv") continue;
    test_parse_cmpfile(e.path(), delim, comment_chars, trim_chars);
  }
}

void test_fopen_error()
{
  test_info("Checking file-open exception ...");
  static volatile int nop_i = 0;
  const auto row_proc = [&](const std::vector<std::string>&, size_t) { nop_i = nop_i + 1; };
  const auto nonexistent_path = "./no-such-file-or-directory.csv";
  test_expect(!std::filesystem::exists(nonexistent_path));
  test_expect_except(csv::csv_parser(row_proc).parse_file(nonexistent_path));
}

void test_parse_string_stop_at_nulchar()
{
  test_info("Checking stop-on-0-character ...");
  static volatile int num_rows = 0;
  auto fields_crammed = std::string();
  auto fields_input = std::string("1,2,3\n4,5,6\r\n,7,8,9\tN,O,T\n");
  std::replace(fields_input.begin(), fields_input.end(), '\t', '\0');
  const auto row_proc = [&](const std::vector<std::string>& fields, size_t) {
    num_rows = num_rows + 1;
    for(const auto& col: fields) fields_crammed += col;
  };
  test_info("Input fields:", fields_input);
  test_expect_noexcept(csv::csv_parser(row_proc).parse(std::string(fields_input)));
  test_expect_eq(num_rows, 3);
  test_expect_eq(fields_crammed, "123456789");
}

void test(const std::vector<std::string>&)
{
  test_fopen_error();
  test_parse_string_stop_at_nulchar();
  test_parse_cmpfile_all("data/comma-notrim", ',', "", "");
  test_parse_cmpfile_all("data/comma-trimsp", ',', "", "\t ");
  test_parse_cmpfile_all("data/comma-comm", ',', "#", "\t ");
}
