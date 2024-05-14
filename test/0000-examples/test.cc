/**
 * @test examples
 *
 * CSV parser/composer usage examples.
 */
#include <testenv.hh>
#include <csv.hh>
#include <system_error>
#include <charconv>
#include <optional>
#include <iomanip>
#include <map>

// Examples are in functions below this namespace.
namespace {

  /**
   * Auxiliary string-to-number conversion
   * function, returns boolean success.
   *
   * @tparam typename T
   * @param const std::string_view s
   * @param T& value
   * @return bool
   */
  template<typename T>
  bool to_number(const std::string_view s, T& value)
  {
    const auto end = s.data() + s.size();
    const auto [p, e] = std::from_chars(s.data(), end, value);
    (void)p;
    return (e == std::errc());
  };

  /**
   * Quoting for line dumps. C-style quoting.
   */
  std::string dump_quote_field(const std::string_view field_text)
  {
    auto s = std::string("\"");
    for(const auto c: field_text) {
      if(c > '"') {
        s += c;
      } else {
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
          case '"':
            {
              s += "\\\"";
              break;
            }
          default:
            {
              s += '\\';
              s += std::to_string(int(c));
            }
        }
      }
    }
    s += '"';
    return s;
  }

  /**
   * Auxiliary function to dump a CSV line with
   * C-style escaped string data.
   */
  void dump_fields(const std::vector<std::string>& fields, size_t line_no)
  {
    auto& os = std::cout;
    os << "[" << std::setw(3) << int(line_no) << "] :";
    for(const auto& s: fields) { os << " " << dump_quote_field(s); }
    os << "\n";
  }

}

// Parse examples ------------------------------------------------------------------

namespace {
  /**
   * Example: Simple inline construction and parsing.
   *
   * Newline characters \r and \n are automatically detected,
   * empty lines are ignored. The row processing function
   * can be specified as lambda.
   *
   * - CSV quotes and corresponding are implicitly handled by
   *   the parser.
   *
   * - The default delimiter is ',' (can be changed via c'tor
   *   arguemnt, optional).
   *
   * - By default, no file head comments are ignored (can be
   *   changed via c'tor arguemnt, optional).
   *
   * - By default, no field trimming is done (can be changed
   *   via c'tor arguemnt, optional).
   */
  void example_parse_string()
  {
    csv::csv_parser([&](const auto& fields, size_t line_no) {
      std::cout << "line " << int(line_no) << ":";
      for(const auto& s: fields) { std::cout << " " << dump_quote_field(s); }
      std::cout << "\n";
    })
      .parse("r1c1,r1c2,r1c3,r1c4\n"         // newline \n
             "r2c1,r2c2,r2c3,r2c4\r"         // newline \r
             "r3c1,r3c2,r3c3,r3c4\r\n"       // newline \r\n, RFC4180
             "r4c1,\"r4c2\",r4c3,r4c4\n"     // quotes, RFC4180
             "r5c1,\"r5\"\"2\",r5c3,r5c4\n"  // quotes with escape "", RFC4180
             "r6c1,\"r6\n2\",r6c3,r6c4\n"    // quotes with newline in the data, RFC4180
             "\n"                            // empty lines ignored, \r\n counts as one newline
             "r8c1,\"r8\n2\",r8c3,r8c4\n"    // next line is 8, line processor not called for line 7
      );
  }

  /**
   * Example: Comments at the start of the file can
   * be ignored by the parser, which is a bit faster
   * than tracking this in the row-processing function.
   */
  void example_parse_header_comments()
  {
    const auto row_processor = dump_fields;

    csv::csv_parser(row_processor, ',', "#;")
      .parse("# Comments at the start will\n"
             "; be ignored\n"
             "r1c1,r1c2,r1c3,r1c4\n"
             "r2c1,r2c2,r2c3,r2c4\n"
             "r3c1,r3c2,r3c3,r3c4\n");
  }

  /**
   * Example: Characters like whitespaces
   * at the start or end of a CSV field can
   * be trimmed of during parsing.
   */
  void example_parse_field_trimming()
  {
    const auto row_processor = dump_fields;
    const auto trim_chars = " \t";
    auto parser = csv::csv_parser(row_processor, ',', "", trim_chars);
    parser.parse("  r1c1 \t ,r1c2, r1c3  , r1c4 \n"
                 " r2c1 ,r2c2,r2c3,r2c4\n"
                 "r3c1, r3c2\t,r3c3,r3c4\n");
  }

  /**
   * Example: Partial parsing (e.g. istream incoming
   * data). To do so, `feed()` it character data,
   * and invoke `finish()` after EOF/EOS to convert
   * the last line (in case the last CSV line did not
   * end with a newline).
   */
  void example_parse_partial_inline()
  {
    const auto incoming =
      std::array{"r1c1,r1c2,r1c3,r1c4\n", "r2c1,r2", "c2,r2c3,r2c4\r", "r3c1,r3c2,r3c3,r", "3c4\r\n"};

    auto parser = csv::csv_parser(dump_fields);
    for(const auto& in: incoming) { parser.feed(in); }
    parser.finish();
  }

  /**
   * Example for inline processing CSV row data and headers.
   *
   * The data file for this example is `world-population.csv`,
   * which is a UN world population record excerpt
   * (https://population.un.org/wpp/Download/Standard/CSV/).
   *
   * Presume we like to retrieve the total population for a year
   * from that. Among the columns, there are data for `Time` (the
   * year), `AgeGrpStart` (age 0 to 100+), and `PopTotal` for
   * the years and ages, so each year has multiple sub-records
   * by age group. To filter only the data for the whole world,
   * the `Location` needs to be checked accordingly.
   *
   * For the total of each year, we like to:
   *
   *  1. Find the year and total population columns from the header names,
   *  2. Make a std::map with a `year => population` mapping.
   *  3. Sum up all population counts (for all age groups) for each year,
   *     filtered by `Location==World`.
   */
  void example_parse_world_population()
  {
    using namespace std;

    // Result map
    auto population_of_year = std::map<int, double>();

    // Row processing variables
    auto num_cols = size_t(0);
    constexpr auto not_found_index = size_t(-1);
    auto year_column = not_found_index;
    auto population_column = not_found_index;
    auto location_column = not_found_index;

    // Row processing function, called for each parsed data row.
    const auto row_processor = [&](const vector<string>& fields, size_t line_no) {
      if(num_cols > 0) {
        // Data columns
        if(fields.size() != num_cols) {
          cerr << "Field vs header size mismatch at line " << int(line_no) << '\n';
        } else if(fields[location_column] == "World") {
          auto year = int();
          auto total = double();
          if(!to_number(fields[year_column], year) || !to_number(fields[population_column], total)) {
            cerr << "Number parsing error at line " << int(line_no) << '\n';
          } else {
            population_of_year[int(year)] += total;  // map operator[] auto initializes elements.
          }
        }
      } else {
        // Header columns
        num_cols = fields.size();
        for(auto col = size_t(0); col < num_cols; ++col) {
          const auto& header = fields[col];
          if(header == "Time") {
            year_column = col;
          } else if(header == "PopTotal") {
            population_column = col;
          } else if(header == "Location") {
            location_column = col;
          }
        }
        if(year_column == not_found_index) {
          throw runtime_error(string("Time column header not found, line ") + to_string(line_no));
        }
        if(population_column == not_found_index) {
          throw runtime_error(string("PopTotal column header not found, line ") + to_string(line_no));
        }
        if(location_column == not_found_index) {
          throw runtime_error(string("Location column header not found, line ") + to_string(line_no));
        }
      }
    };

    // Parser configuration and file reading.
    {
      const auto file_path = "data/world-population.csv";
      const auto delimiter = ',';
      const auto header_comment_characters = "#";
      const auto trim_characters = " \t";
      try {
        cout << "Parsing " << file_path << " ...\n";
        // Main call to the parser:
        csv::csv_parser(row_processor, delimiter, header_comment_characters, trim_characters).parse_file(file_path);
        cout << "World popupation in Giga-people accumulated for years:\n";
        static constexpr auto giga = 1e6;
        for(const auto& kv: population_of_year) {
          cout << " - " << kv.first << ": " << std::setprecision(2) << (double(kv.second) / giga) << "G\n";
        }
      } catch(const exception& ex) {
        cerr << "Parsing file " << file_path << ": " << ex.what() << "\n";
      }
    }
  }

  // Compose examples ----------------------------------------------------------------

  /**
   * The composer is fed with string-like value
   * containers, and composes CSV lines from these
   * data:
   *
   * Specified when constructing is:
   *
   *  - the output function (callback for each data row),
   *  - the CSV delimiter to be used (optional, default ',')
   *  - the newline sequence (optional, default "\r\n", RFC)
   *
   * Using `define_columns()`, you set:
   *
   *  - the number of columns,
   *  - the indices (1 to N, not 0 to N-1) of the columns that
   *    shall always be quoted (optional).
   *
   * If field escaping is needed for a field, this is
   * implicitly done, even if the column is not marked for
   * forced-quoting.
   */
  void example_compose()
  {
    // Example data set -> array[row][col]
    const auto data_rows = std::array{
      std::array{"column1", "column2", "column3"}, // col1 quoted (see below)
      std::array{    "ABC",     "def",     "ghi"}, // col1 quoted (see below)
      std::array{   " ABC",    "def ",   " ghi "}, // implicit quoting
      std::array{ " A\"BC",   "de\nf", "gh\r\ni"}, // implicit quoting
      std::array{    "jkl",     "mno",     "pqr"}, // col1 quoted (see below)
      std::array{    "stu",     "vwx",     "yz0"}  // col1 quoted (see below)
    };

    // Output function: print to stdout.
    const auto out_fn = [](const std::string& line) { std::cout << "  |> " << line; };

    // Compose, args ',' and "\r\n" are optional (default according to RFC).
    auto composer = csv::csv_composer(out_fn, ',', "\r\n");
    composer.define_columns(3, std::array{1});  // 3 columns expected, column 1 always quoted
    for(const auto& row: data_rows) { composer.feed(row); }
  }

  // Parse readme example ------------------------------------------------------------

  void parser_example()
  {
    const auto row_processor = [](const auto& fields, size_t line_no) {
      std::cout << "[" << int(line_no) << "]";
      for(const auto& field: fields) std::cout << " | " << field;
      std::cout << "\n";
    };

    // Example: File parsing, explicit arguments.
    {
      const auto file_path = "data/my_data.csv";  // filesystem::path to the file
      const auto delimiter = ',';                 // CSV separator (optional ',' = default)
      const auto header_comments = "";            // File head comment line start chars (optional "" = default)
      const auto trim_chars = "";                 // Field trim characters (optional "" = default)

      // Construct and use parser.
      auto parser = csv::csv_parser(row_processor, delimiter, header_comments, trim_chars);
      parser.parse_file(file_path);
      std::cout << "+++\n";
    }

    // Example: String parsing, default CSV, in-line construction and use.
    {
      const auto data = "c1,c2,c3\n0,1,2\n3,4,5\n6,7,8";
      csv::csv_parser(row_processor).parse(data);
      std::cout << "+++\n";
    }

    // Example: Chunk-wise partial parsing (e.g. from istream).
    {
      const auto data_chunks = std::array{"c1,c2,c3\n0,1,", "2\n3,4,5\n6,", "7,8"};

      // Instantiate
      auto parser = csv::csv_parser(row_processor);
      // Feed incoming data
      for(const auto& chunk: data_chunks) { parser.feed(chunk); }
      // Process remaining data after the stream is EOF.
      parser.finish();

      std::cout << "+++\n";
    }

    // Example: Field trimming and header comment ignoring.
    // (This is not RFC4180 compliant, but useful nonetheless)
    {
      const auto data = "| Comment 1\n# Comment 2\nc1 ;  c2  ; c3\n0;\t1;2\n3;4;5\n6 ; 7 ; 8";

      // Separator=";"
      // Lines at the top starting with '#' or '|' are ignored.
      // Each field is left and right trimmed tabs or spaces.
      csv::csv_parser(row_processor, ';', "#|", " \t").parse(data);
      std::cout << "+++\n";
    }
  }

}

// Main ----------------------------------------------------------------------------

/**
 * Test main() function.
 */
void test(const std::vector<std::string>&)
{
  test_note("#--------------------------------------------------------");
  test_note("example_parse_string()");
  test_note("#--------------------------------------------------------");
  test_note("#");
  example_parse_string();

  test_note("#");
  test_note("#--------------------------------------------------------");
  test_note("example_parse_header_comments()");
  test_note("#--------------------------------------------------------");
  example_parse_header_comments();

  test_note("#");
  test_note("#--------------------------------------------------------");
  test_note("example_parse_field_trimming()");
  test_note("#--------------------------------------------------------");
  example_parse_field_trimming();

  test_note("#");
  test_note("#--------------------------------------------------------");
  test_note("example_parse_partial_inline()");
  test_note("#--------------------------------------------------------");
  example_parse_partial_inline();

  test_note("#");
  test_note("#--------------------------------------------------------");
  test_note("example_parse_world_population()");
  test_note("#--------------------------------------------------------");
  example_parse_world_population();

  test_note("#");
  test_note("#--------------------------------------------------------");
  test_note("example_compose()");
  test_note("#--------------------------------------------------------");
  example_compose();

  test_note("#");
  test_note("#--------------------------------------------------------");
  test_note("README example_parser()");
  test_note("#--------------------------------------------------------");
  parser_example();
}
