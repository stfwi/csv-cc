/**
 * @test parse-file-perf
 *
 * csv::csv_parser performance indication.
 * Generates random CSV files with pre-defined
 * sized (actual files may be a little bigger),
 * and measures the parsing time including
 * file i/o handling. Column count is random.
 *
 * Repeats the measurements and prints the summary.
 *
 * For more extensive tests, run the test ELF/EXE
 * file with one numeric argument:
 *
 * - test.elf(/.exe) 0 -> to 10MB
 * - test.elf(/.exe) 1 -> to 100MB
 * - test.elf(/.exe) 2 -> to 1GB
 * - test.elf(/.exe) 2 -> to 5GB
 * - test.elf(/.exe) 3 -> to 10GB
 *
 * Default is 0 for normal functional testing.
 */
#include <testenv.hh>
#include <include/csv.hh>
#include <iostream>
#include <memory>
#include <string>
#include <chrono>

double test_perf_cycle(std::filesystem::path path)
{
  using namespace std;

  auto accumulated_content_length = size_t(0);
  const auto read_fields = [&](const std::vector<std::string>& fields, size_t line_no) {
    for(const auto& s: fields) accumulated_content_length += s.size();
    (void)line_no;
  };

  const auto start_time = chrono::high_resolution_clock::now();
  double bytes_processed = double(filesystem::file_size(path));
  test_expect_noexcept(csv::csv_parser(read_fields, ',').parse_file(path));
  const auto test_time = chrono::high_resolution_clock::now() - start_time;
  const auto secs = 1e-6 + double(chrono::duration_cast<chrono::milliseconds>(test_time).count()) * 1e-3;
  static constexpr double mb_scale = 1.0 / (1024 * 1024);
  const auto mbytes_per_sec = (bytes_processed / secs) * mb_scale;
  test_note("String sizes of all cells accumulated: " << long(accumulated_content_length) << ".");
  test_note("Test time:" << secs << ", payload:" << long(bytes_processed * mb_scale) << "MB, MB/s: " << mbytes_per_sec);
  return mbytes_per_sec;
}

void test(const std::vector<std::string>& args)
{
  using namespace std;

  // Performance test mainly relevant for machine specific perf tests, hence, CLI args can be
  // used to increase the test file size.
  const int size_scale = (args.size() > 0) ? (::atoi(args.front().c_str())) : (0);
  // NOLINTBEGIN
  auto csv_file_sizes_kb = std::vector{size_t(1024 * 1), size_t(1024 * 10)};  // default for CI testing: up to 10MB.
  if(size_scale > 0) csv_file_sizes_kb.push_back(size_t(1024 * 100));         // test.elf(/.exe) 1 -> to 100MB
  if(size_scale > 1) csv_file_sizes_kb.push_back(size_t(1024 * 1000));        // test.elf(/.exe) 2 -> to 1GB
  if(size_scale > 2) csv_file_sizes_kb.push_back(size_t(1024 * 5000));        // test.elf(/.exe) 2 -> to 5GB
  if(size_scale > 3) csv_file_sizes_kb.push_back(size_t(1024 * 10000));       // test.elf(/.exe) 3 -> to 10GB
  // NOLINTEND

  // Some more currently fixed params until other checks are needed.
  const auto num_pref_test_iterations = int(6);
  const auto csv_delimiter = char(',');
  const auto csv_num_cols = std::size_t(8);
  const auto csv_header = std::string("# comment 1\n# comment 2\n# comment 3\n# comment 4\n");
  const auto csv_max_field_length = std::size_t(16);
  const auto csv_field_character_pool = te::rnd_pool_ascii();

  auto perf_summary = std::vector<std::string>();

  for(const auto& csv_file_size_kb: csv_file_sizes_kb) {
    test_info("#----------------------------------------");
    test_info("Processing for file size ", csv_file_size_kb, "...");
    const auto csv_file_path = te::make_random_csv_file(
      "tcsv-",
      csv_file_size_kb,
      csv_num_cols,
      csv_delimiter,
      csv_header,
      csv_max_field_length,
      std::string(csv_field_character_pool));
    auto stats = std::vector<double>();
    for(int i = num_pref_test_iterations; i; --i) {
      test_expect_noexcept(stats.push_back(test_perf_cycle(csv_file_path)));
    }
    const auto mean_time = std::accumulate(stats.begin(), stats.end(), double(0), [](auto a, auto b) { return a + b; })
                           / double(stats.size());
    test_info("Average rate:", mean_time, "MB/s");
    perf_summary.push_back(to_string(mean_time) + string("MB/s : ") + csv_file_path.filename().string());
    if((sw::utest::test::num_fails() == 0) && filesystem::is_regular_file(csv_file_path)) {
      test_info("Removing tmp file ", csv_file_path);
      filesystem::remove(csv_file_path);
    }
  }
  test_info("#----------------------------------------");
  test_info("Parse performance summary:");
  for(auto& s: perf_summary) { test_info(s); }
}
