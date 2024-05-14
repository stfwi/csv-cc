![Test](https://github.com/stfwi/csv-cc/actions/workflows/ci-test.yml/badge.svg?branch=main)
![Code Analysis](https://github.com/stfwi/csv-cc/actions/workflows/ci-analysis.yml/badge.svg?branch=main)
![Coverage](https://github.com/stfwi/csv-cc/actions/workflows/ci-coverage.yml/badge.svg?branch=main)

## `csv.hh` - C++ Single Header CSV Parser/Composer

MIT licensed minimalistic CSV parser and composer. The main
file is located at `include/csv.hh` of this repository.

  - STL-only: No dependencies except the STL.

  - Single-threaded.

  - RFC4180 by default, aspects like delimiter/separator
    selection, header-comment ignoring, or field trimming
    are optional.

  - The implementation covers only CSV related aspects
    (separation of concerns). Interpretation of field data,
    header selection, or the decision if a different record
    field count is okay to use or a strict error, is done
    the user code.

  - Simplicitly/readability/performance balance: Leaving the
    compiler enough space to optimize without specifying everything
    as template argument. Avoid allocations where possible,
    allocate where sensible for further data processing.

### Parser Examples

In addition to the examples in `test/0000-examples`, a brief
usage example and feature description is given here.

```c++
#include <csv.hh>
#include <iostream>
#include <string>
#include <vector>
#include <array>


void parser_example()
{
  // Your processing function for each parsed row, re-used in multiple
  // blocks below.
  const auto row_processor = [&](const std::vector<std::string>& fields, size_t line_no) {
    std::cout << "[" << int(line_no) << "]";
    for(const auto& field: fields) std::cout << " | " << field;
    std::cout << "\n";
  };

  // Example: File parsing, explicit arguments.
  // Output:
  //
  //    [1] | col1 | col2 | col3
  //    [2] | r1c1 | r1c2 | r1c3
  //    [3] | r2c1 | r2c2 | r2c3
  //    +++
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
  // Output:
  //
  //    [1] | c1 | c2 | c3
  //    [2] | 0 | 1 | 2
  //    [3] | 3 | 4 | 5
  //    [4] | 6 | 7 | 8
  //    +++
  {
    const auto data = "c1,c2,c3\n0,1,2\n3,4,5\n6,7,8";
    csv::csv_parser(row_processor).parse(data);
    std::cout << "+++\n";
  }

  // Example: Chunk-wise partial parsing (e.g. from istream).
  // Output:
  //
  //    [1] | c1 | c2 | c3
  //    [2] | 0 | 1 | 2
  //    [3] | 3 | 4 | 5
  //    [4] | 6 | 7 | 8
  //    +++
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
  // Output:
  //
  //    [3] | c1 | c2 | c3
  //    [4] | 0 | 1 | 2
  //    [5] | 3 | 4 | 5
  //    [6] | 6 | 7 | 8
  //    +++
  {
    const auto data = "| Comment 1\n# Comment 2\nc1 ;  c2  ; c3\n0;\t1;2\n3;4;5\n6 ; 7 ; 8";

    // Separator ';'
    // Lines at the top starting with '#' or '|' are ignored.
    // Each field is left and right trimmed tabs or spaces.
    csv::csv_parser(row_processor, ';', "#|", " \t").parse(data);
    std::cout << "+++\n";
  }

}
```

### Parser Description

The `csv_parser` implements the following RFC4180 aspects:

  - The specified record separator is `CRLF`, however, "implementation should
    be aware that implementations may use other values". Therefore, `CR` (`\r`),
    `LF` (`\n`), and `CRLF` (`\r\n`) are accepted as newline, and empty lines
    are ignored.

  - Field quoting character is `"`, e.g. `"aaa","bbb"`.

  - Field quote escape sequence is `""`, e.g. `"a""aa","bbb"`.

  - Newlines (`\n`, or `\r`) in quoted fields are part of the record.

  - (CSV header data can be handled in the row-processor function).

Additional considerations:

  - The parser only throws on underlying container errors (out of
    memory, etc), or in `parse_file()` on `fstream` error.

Performance considerations:

  - As file I/O has a significant performance impact, the parser reads
    in chunks of a pre-defined size (for `csv_parser` 1MB). The chunk
    buffer is only allocated once. You can change its size by creating
    an own specialization of the underlying `basic_parser` class
    template:

    ```c++
    using parser128kb = csv::detail::basic_parser<128, std::string, std::vector<std::string>>;
    ```

  - Parsing performance: Although allocations are circumvented as good as
    possible, additional performance optimization can be done by replacing
    the `std::string` and `std::vector<std::string>` above with an own
    string type with pool allocator - if needed.

  - For performance tests on your machine, you can use the test
    `0002-parse-file-perf`, which creates random CSV files with different
    sizes, and measures the parsing time over multiple cycles. By default,
    the test runs only with a 1MB and 10MB file (to save CI performance).
    Using the first command line argument of the test executable, you push
    additional file sizes (cumulatively):

    ```sh
    # Arg: 1=100MB 2=1GB 3=5GB 4=10GB
    make
    cd build/test/0002-parse-file-perf/
    ./test.elf 1
    # Windows: test.exe 1
    ```

    The output looks like:

    ```
    [info] [@./test/microtest.hh:1406] compiler: gcc (11.4.0), std=c++17, platform: linux, scm=aa7c4fb
    [note] [@test/0002-parse-file-perf/test.cc:78] #----------------------------------------
    [note] [@test/0002-parse-file-perf/test.cc:79] Processing for file size 1024...
    [note] [@./test/testenv.hh:125] Creating test CSV file "tcsv--1024kb-8cols-44d.csv" with 48 bytes header/prefix.
    [pass] [@./test/testenv.hh:127] num_cols > 0
    ...
    ...
    [note] [@test/0002-parse-file-perf/test.cc:101] #----------------------------------------
    [note] [@test/0002-parse-file-perf/test.cc:102] Parse performance summary:
    [note] [@test/0002-parse-file-perf/test.cc:103] 343.060866MB/s : tcsv--1024kb-8cols-44d.csv
    [note] [@test/0002-parse-file-perf/test.cc:103] 305.832213MB/s : tcsv--10240kb-8cols-44d.csv
    [note] [@test/0002-parse-file-perf/test.cc:103] 312.577797MB/s : tcsv--102400kb-8cols-44d.csv
    [note] [@test/0002-parse-file-perf/test.cc:103] 314.110819MB/s : tcsv--1024000kb-8cols-44d.csv
    [note] [@test/0002-parse-file-perf/test.cc:103] 315.795272MB/s : tcsv--5120000kb-8cols-44d.csv
    [PASS] All 90 checks passed, 0 warnings.
    ```

### Composer Examples

In addition to the examples in `test/0000-examples`, a brief
usage example and feature description is given here.

```c++
#include <csv.hh>
#include <array>

void example_compose()
{
  // Example data set -> array[row][col]
  auto data_rows = std::array{
    std::array{"column1", "column2", "column3"}, // col1 quoted (see below)
    std::array{    "ABC",     "def",     "ghi"}, // col1 quoted (see below)
    std::array{   " ABC",    "def ",   " ghi "}, // implicit quoting
    std::array{ " A\"BC",   "de\nf", "gh\r\ni"}, // implicit quoting
    std::array{    "jkl",     "mno",     "pqr"}, // col1 quoted (see below)
    std::array{    "stu",     "vwx",     "yz0"}  // col1 quoted (see below)
  };

  // Output function: print to stdout.
  const auto out_fn = [&](const std::string&& line) { std::cout << "  |> " << line; };

  // Compose, args ',' and "\r\n" are optional (default accoeding to RFC).
  auto composer = csv::csv_composer(out_fn, ',', "\r\n");
  composer.define_columns(3, std::array{1});  // 3 columns expected, column 1 always quoted
  for(const auto& row: data_rows) { composer.feed(row); }
}
```

### Composer Description

The `csv_composer` implements the following RFC4180 aspects:

  - The default record separator is `CRLF` (`\r\n`).

  - Field quoting character is `"`, e.g. `"aaa","bbb"`.

  - Field quote escape sequence is `""`, e.g. `"a""aa","bbb"`.

  - Fields containing `"`, `\n`, or `\r` are implicitly quoted.

Additional composer considerations:

  - Use of exceptions for invalid data. That is,

    - if `define_columns()` is invoked multiple times,

    - if in `define_columns()` the defined column count
      is zero,

    - if in `define_columns()` the definition of mandatory
      field quoting indices exceed the valid column index
      range (1 to N),

    - or if the number of columns in `feed()` do not match
      the defined column count.

    Using exceptions here can be considered to be okay, since c++
    exception handling has been optimized, and an exception thrown
    here indicates a bug. When feeding a `std::array`, or a vector
    with predictable size, your compiler will most likely eliminate
    the column count check entirely.

+++
