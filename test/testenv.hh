/**
 * @file testenv.hh
 *
 * Microtest harness configuration and auxiliary
 * functions for testing.
 */
#ifndef TESTING_ENVIRONMENT_HH
#define	TESTING_ENVIRONMENT_HH
//------------------------------------------------------------------------------------------------
// Microtest settings
#define WITH_MICROTEST_MAIN           /* opt-in: int main(){} generation */
#define WITH_MICROTEST_GENERATORS     /* opt-in: sequence and container generation functions */
#define WITH_MICROTEST_ANSI_COLORS    /* opt-in: ANSI coloring for console/TTY out streams */
//#define WITHOUT_MICROTEST_RANDOM    /* opt-out: No utest::random() functions */
//#define WITH_MICROTEST_TMPDIR       /* opt-in: !experimental! Temporary directory creation and handling */
//#define WITH_MICROTEST_TMPFILE      /* opt-in: !experimental1 Temporary file creation and handling */
#include "microtest.hh"
//------------------------------------------------------------------------------------------------

// CSV test specific test harness auxiliary functions.
#include <include/csv.hh>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>


namespace te { namespace {

  inline std::string csv_escape_joined_row_fields(const std::vector<std::string>& fields, size_t line_no)
  {
    auto s = std::to_string(int(line_no)) + " [";
    for(auto& field: fields) {
      s += '`';
      for(auto c: field) {
        switch(c) {
          case '\n':
            {
              s += "\\n";
              break;
            }
          case '\r':
            {
              s += "\\r";
              break;
            }
          case '\t':
            {
              s += "\\t";
              break;
            }
          case '\0':
            {
              s += "\\0";
              break;
            }
          case '\\':
            {
              s += "\\\\";
              break;
            }
          default:
            {
              s += c;
              break;
            }
        }
      }
      s += "`,";
    }
    s.pop_back();
    s += "]";
    return s;
  }


  inline /*constexpr*/ std::string rnd_pool_ascii()
  {
    static constexpr auto del_char = char(0x7f);
    auto s = std::string();
    for(char c=' '; c < del_char; ++c) { s.push_back(c); }
    return s;
  }

  inline /*constexpr*/ std::string rnd_pool_ascii_with_newline()
  { return rnd_pool_ascii() + "\n\r"; }


  inline std::string make_random_csv_row(
    const csv::csv_composer& composer, const size_t num_cols,
    const size_t max_field_length, const std::string_view field_character_pool
  )
  {
    using namespace std;
    using namespace sw::utest;

    const auto field_value = [&](){
      const auto num_chars = random<string::size_type>(0, max_field_length);
      auto s = string(num_chars, ' ');
      for(auto& c: s) { c = field_character_pool[random<string::size_type>(0, field_character_pool.size()-1)]; }
      return composer.escape(s);
    };

    auto line = std::string(field_value());
    for(size_t i=1; i<num_cols; ++i) {
      line += composer.delimiter();
      line += field_value();
    }
    line += "\r\n"; // rfc4180: CRLF
    return line;
  }

  inline std::filesystem::path make_random_csv_file(
    const std::string_view path_prefix, const size_t file_size_kb,
    const size_t num_cols, const char delimiter,
    const std::string_view prefix_or_header,
    const size_t max_field_length,
    const std::string_view field_character_pool
  )
  {
    using namespace std;
    using namespace sw::utest;
    const auto filename = string(path_prefix) + "-" + to_string(int(file_size_kb)) + "kb-" + to_string(int(num_cols)) + "cols-" + to_string(int(delimiter)) + "d.csv";
    const auto path = std::filesystem::path(filename);
    test_info("Creating test CSV file ", path, " with ", prefix_or_header.size(), " bytes header/prefix.");

    if(!test_expect_cond(num_cols > 0)) return "";
    if(!test_expect_cond(file_size_kb > 0)) return "";
    const auto stop_size = file_size_kb * 1024;
    const auto composer = csv::csv_composer(csv::csv_composer::no_output, delimiter);

    auto rand_csv_file_stream = std::ofstream(path, ios::binary);
    test_expect(rand_csv_file_stream.good());
    if(!prefix_or_header.empty()) {
      rand_csv_file_stream.write(prefix_or_header.data(), std::streamsize(prefix_or_header.size()));
      rand_csv_file_stream << '\n';
    }

    auto n_lines = size_t(0);
    auto size_b = prefix_or_header.size();
    auto rnd_row_chunk = std::string();
    constexpr auto first_100kb = size_t(1024 * 100);
    auto chunk_size = size_t(0);
    auto chunk_lines = size_t(0);

    // Create random first 100kb
    {
      auto ss = std::stringstream();
      const auto end_chunk = std::min(first_100kb, stop_size);
      while(chunk_size < end_chunk) {
        const auto line = make_random_csv_row(composer, num_cols, max_field_length, field_character_pool);
        chunk_size += line.size();
        ++chunk_lines;
        ss.write(line.data(), std::streamsize(line.size()));
      }
      rnd_row_chunk = ss.str();
    }

    while(size_b < stop_size) {
      n_lines += chunk_lines;
      size_b += chunk_size;
      rand_csv_file_stream.write(rnd_row_chunk.data(), std::streamsize(rnd_row_chunk.size()));
    }

    const auto fsz = std::round(double(std::filesystem::file_size(path))/(1024*1024/10)) / 10;
    test_info("Created test CSV file ", path, " has ", n_lines, " data lines, total size: ", fsz, "MB.");
    return path;
  }

}}

#endif
