/**
 * @file csv.hh
 * @author Stefan Wilhelm (stfwi)
 * @version v1.1 (modernized to c++17)
 * @license MIT
 * @standard >= c++17
 * @platform linux, bsd, windows
 * -----------------------------------------------------------------------------
 *
 * Minimalistic, STL-only CSV parser and composer.
 *
 * --------------------------------------------------------------------------
 * +++ MIT license +++
 * Copyright (c) 2018-2024, Stefan Wilhelm <cerbero s@atwilly s.de>
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions: The above copyright notice and
 * this permission notice shall be included in all copies or substantial portions
 * of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * --------------------------------------------------------------------------
 */
// clang-format off
// NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members): No object assignment allowed, const members totally ok.
#ifndef SW_CSV_PARSER_HH
#define	SW_CSV_PARSER_HH
#include <functional>
#include <filesystem>
#include <algorithm>
#include <iterator>
#include <numeric>
#include <fstream>
#include <memory>
#include <string>


/**
 * CSV parsing.
 */
namespace csv { namespace {

  namespace detail {

    /**
     * CSV parser class template, tune your performance
     * vs memory consumption via `ReadBufferSizeKb`, which
     * has a notable performance impact due to stream
     * reading.
     */
    template<
      size_t ReadBufferSizeKb,          // File reading chunk size.
      typename StringType,              // String type used, @concept: must be std::string like.
      typename StringViewContainerType  // Container<StringView> type used, @concept: must be ramdom access and a view to StringType as value_type.
    >
    class basic_parser
    {
    public:

      using string_type = StringType;
      using char_type = typename string_type::value_type;
      using string_view_type = std::basic_string_view<char_type>;
      using string_view_container_type = StringViewContainerType;
      using row_handler_type = std::function<void(const string_view_container_type& fields, size_t line_no)>;

      static constexpr size_t read_buffer_size_kb = ReadBufferSizeKb;

    public:

      basic_parser() = delete;
      basic_parser(const basic_parser&) = delete;
      basic_parser(basic_parser&&) noexcept = default;
      basic_parser& operator=(const basic_parser&) = delete;
      basic_parser& operator=(basic_parser&&) noexcept = default;
      ~basic_parser() noexcept = default;

      /**
       * CSV parser constructor (the only allowed one).
       *
       * @param on_row Function invoked for each CSV row.
       * @param [csv_delimiter] The CSV separator character.
       * @param [header_comment_characters] Leading lines starting with one of the characters in the string will be ignored.
       * @param [trim_characters] Characters to be trimmed off at the start and end of each field (CSV entry). Whitespaces are often trimmed.
       */
      explicit basic_parser(
        const row_handler_type on_row,
        const char_type csv_delimiter = ',',
        const string_view_type header_comment_characters = string_view_type(""),
        const string_view_type trim_characters = string_view_type("")
      )
      : row_handler_(on_row),
        delimiter_(csv_delimiter),
        header_comment_chars_(header_comment_characters),
        trim_chars_(trim_characters),
        current_record_(),
        buffer_(),
        current_field_(),
        line_no_(),
        n_rows_()
      {
        // Support for < c++20: Explicit checks, no use of concepts yet:
        static_assert(std::is_same<string_view_type, typename string_view_container_type::value_type>::value, "StringContainerType has to have StringType as elements.");
        static_assert(std::is_default_constructible<string_view_container_type>::value, "StringContainerType must be a default-constructible dynamic sized container.");
        // Also: wstring not needed by anyone, string with custom allocator maybe:
        static_assert(std::is_same<char, char_type>::value, "wstring not supported. Widen/narrow in your code.");
      }

    public:

      /**
       * Clears the internal state (but not the
       * construction settings), so that a new
       * CSV data set can be processed.
       * @return basic_parser&
       */
      basic_parser& clear()
      {
        current_record_ = string_view_container_type();
        buffer_ = string_type();
        current_field_ = string_range();
        line_no_ = 0;
        n_rows_ = 0;
        return *this;
      }

      /**
       * Partial CSV parsing, invokes `row_handler` directly
       * when a data line is completed. Leaves unfinished line
       * data in the internal buffer for the next feed cycle.
       * Use `finish()` to ensure that the last line of file
       * or stream is not lost.
       * @param string_type&& csv_text
       * @return basic_parser&
       */
      basic_parser& feed(string_type&& csv_text)
      { return push(std::move(csv_text), true); }

      /**
       * Adds the internally buffered line in case there is no
       * trailing newline in the CSV data before end of input.
       * Use `finish()` after all data have been processed using
       * `feed()`. This method is not automatically invoked in
       * the destructor, as the (your) `row_handler` may throw.
       * @return basic_parser&
       */
      basic_parser& finish()
      { feed("\n"); return *this; }

      /**
       * Parses and finishes (@see `feed()` and `finish()`) a
       * complete CSV text.
       * @param string_type&& csv_text
       */
      void parse(string_type&& csv_text)
      { clear(); feed(std::move(csv_text)); finish(); }

      /**
       * Reads and parses a CSV (regular) file given by
       * its filesystem path. Throws on file reading
       * or memory errors.
       *
       * @param const std::filesystem::path& path
       * @throw std::exception
       */
      void parse_file(const std::filesystem::path& path)
      {
        using namespace std;
        clear();
        auto fis = ifstream(path, ifstream::binary);
        if(!fis) {
          throw runtime_error("Failed to open CSV file.");
        }
        while(fis.good()) {
          constexpr auto chunk_size = string::size_type(read_buffer_size_kb * 1024);
          auto csv_text = string(chunk_size, '\0');
          fis.read(csv_text.data(), std::streamsize(csv_text.size()-1));
          const auto n_read = string::size_type(fis.gcount());
          if(n_read <= 0) continue;
          csv_text.resize(n_read+1);
          push(std::move(csv_text), false);
        }
        finish();
        if(!fis.eof()) throw runtime_error("Not all CSV file data could be read.");
      }

    protected:

      /**
       * Internal partial CSV parsing (@see `feed()`), where the
       * peek-ahead reserve memory can be added before to circumvent
       * unnecessary performance penalty due to re-allocation.
       * @param string_type&& csv_text
       * @param bool with_lookahead
       * @return basic_parser&
       */
      basic_parser& push(string_type&& csv_text, bool with_lookahead)
      {
        using namespace std;
        if(csv_text.empty()) return *this;
        if(with_lookahead) csv_text.push_back('\0');
        auto cursor = csv_text.begin();
        const auto end = csv_text.end()-1;

        const auto peek = [&](){
          return *cursor;
        };

        const auto skip = [&](){
          ++cursor;
          return peek();
        };

        const auto consume = [&](){
          buffer_.push_back(*cursor);
          return skip();
        };

        const auto consume_quoted = [&](){
          auto c = peek();
          for(; c; c = consume()) {
            if(c != '"') continue;
            // RFC4180 double-quote escape, consume second quote.
            c = skip();
            if(c != '"') break;
          }
          return c;
        };

        const auto consume_unquoted = [&](){
          // RFC4180: Quotes are only registered directly after the delimiter or the start
          // of line, so any quotes in the field are accepted as normal character.
          auto c = consume();
          for(; c; c=consume()) {
            if((c==delimiter_) || (c=='\r') || (c=='\n')) break;
          }
          return c;
        };

        const auto purge_comments = [&](){
          if(header_comment_chars_.empty()) return;
          // Ignores content, as long as lines are empty or starting with
          // one of the comment_chars.
          auto c = peek();
          while(c) {
            if((c == '\n') || (c == '\r')) {
              ++line_no_;
              c = skip();
            } else if(header_comment_chars_.find(c) != header_comment_chars_.npos) {
              while(c && (c != '\n') && (c != '\r')) c = skip(); // skip whole line
            } else {
              break;
            }
          }
        };

        const auto trim_field = [&](string_view_type s) -> string_view_type {
          if(trim_chars_.empty() || s.empty()) { return s; }
          const auto npos = s.npos;
          auto epos = s.size();
          auto spos = typename string_view_type::size_type(0);
          while((epos > 0) && trim_chars_.find(s[epos-1]) != npos) --epos;
          while((spos < epos) && trim_chars_.find(s[spos]) != npos) ++spos;
          if((spos == 0) && (epos == s.size())) {
            return s;
          } else if(epos <= spos) {
            return string_view_type("", 0);
          } else {
            return string_view_type(&s[spos], epos-spos);
          }
        };

        const auto push_field = [&](){
          current_field_.end = buffer_.size();
          fields_.emplace_back(current_field_);
          current_field_.start = current_field_.end;
        };

        const auto finish_line = [&](){
          const auto rsize = buffer_.size();
          if(((rsize==0) || (current_field_.start >= rsize)) && fields_.empty()) return false;
          push_field();
          for(const auto& e:fields_) current_record_.push_back(trim_field(string_view_type(&buffer_[e.start], e.end-e.start)));
          row_handler_(current_record_, line_no_);
          current_record_.clear();
          fields_.clear();
          buffer_.clear();
          current_field_ = string_range();
          ++n_rows_;
          return true;
        };

        if(n_rows_ == 0) purge_comments();

        while(cursor != end) {
          const auto c = peek();
          if(c == delimiter_) {
            skip();
            push_field();
          } else if((c == '\r') || (c == '\n')) {
            const auto cn = skip(); // RFC4180 specifies \r\n, but we accept CR, LF, or CRLF as newline.
            if((c == '\r') && (cn == '\n')) skip();
            ++line_no_;
            if(!finish_line()) continue;
          } else if(c == '"') {
            if((!skip()) || (!consume_quoted())) break;
          } else if(c) {
            consume_unquoted();
          } else {
            break; // '\0', pushed above -> end of string.
          }
        }
        return *this;
      }

    private:

      struct string_range { typename string_type::size_type start; typename string_type::size_type end; };

      const row_handler_type row_handler_;          // Function invoked for each CSV row.
      const char_type delimiter_;                   // The CSV separator character.
      const string_type header_comment_chars_;      // Leading lines starting with one of the characters in the string will be ignored.
      const string_type trim_chars_;                // Characters to be trimmed off at the start and end of each field.

      string_view_container_type current_record_;   // Internal state: Container passed to `row_handler_`, only re-allocated on size increase.
      std::vector<string_range> fields_;            // Internal state: Fields registered so far for the current CSV line.
      string_type buffer_;                          // Internal state: Currently unfinished record character buffer.
      string_range current_field_;                  // Internal state: Start position of the current field in the `buffer_`.
      size_t line_no_;                              // Internal state: Current line number in the CSV file.
      size_t n_rows_;                               // Internal state: Number of data rows parser so far.
    };

  }

  /**
   * CSV parser default specialization.
   */
  using csv_parser = detail::basic_parser<1024, std::string, std::vector<std::string_view>>; // NOLINT Default: byte string, 1MB file reading buffer cap.

}}


/**
 * CSV composing.
 */
namespace csv { namespace {

  namespace detail {

    /**
     * CSV composer class template.
     */
    template<
      typename StringType,         // String type used.
      typename QuoteContainerType  // Container<int> type used, @concept: must be random access and `int` as value_type.
    >
    class basic_composer
    {
    public:

      using string_type = StringType;
      using char_type = typename string_type::value_type;
      using string_view_type = std::basic_string_view<char_type>;
      using row_handler_type = std::function<void(const string_type& line)>;
      using quote_cols_type = QuoteContainerType;

      static void no_output(const string_type&){}

    public:

      basic_composer() = delete;
      basic_composer(const basic_composer&) = delete;
      basic_composer(basic_composer&&) noexcept = default;
      basic_composer& operator=(const basic_composer&) = delete;
      basic_composer& operator=(basic_composer&&) noexcept = default;
      ~basic_composer() noexcept = default;

      /**
       * CSV composer constructor.
       *
       * @param const row_handler_type on_row Invoked for every composed line. Use it to output the line.
       * @param const char_type [csv_delimiter] The CSV separator character.
       * @param const string_view_type [newline_seq] Character sequence used for line breaks.
       */
      explicit basic_composer(
        const row_handler_type on_row,
        const char_type csv_delimiter = char_type(','),
        const string_view_type newline_seq = string_view_type("\r\n")
      ) :
        row_handler_(on_row), delimiter_(csv_delimiter), newline_(newline_seq),
        quote_cols_(), num_cols_()
      {
        static_assert(std::is_same<char, char_type>::value, "wstring not supported. Widen/narrow in your code.");
        static_assert(std::is_same<int, typename quote_cols_type::value_type>::value, "Container for forced-quoted column definitions must have `int` as element type.");
      }

    public:

      /**
       * Returns the delimiter used by this
       * composer.
       * @return char_type
       */
      char_type delimiter() const noexcept
      { return delimiter_; }

      /**
       * Returns the line separator used by this
       * composer.
       * @return const string_type&
       */
      const string_type& newline() const noexcept
      { return newline_; }

    public:

      /**
       * Quotes a text, irrespective if it needs to be.
       * Quoting means enclosing the string with `"`,
       * and replacing all `"` with the CSV escape
       * sequence `""`.
       */
      static string_type quote(const string_view_type field_text)
      {
        auto s = string_type("\"");
        for(const auto c:field_text) { s += c; if(c=='"') s+='"'; }
        s += '"';
        return s;
      }

      /**
       * Quotes a text, if:
       *  - it contains non-printable characters (also newlines),
       *  - or the delimiter of this composer instance,
       *  - or quotes (`"`),
       *  - or starts or ends with spaces (` `),
       *  - or if the character is not in ASCII range (char > 0x7f).
       *
       * @param const string_view_type field_text
       * @return string_type
       */
      string_type escape(const string_view_type field_text) const
      {
        if(!field_text.empty()) {
          if((field_text.front() == ' ') || (field_text.back() == ' ')) return quote(field_text);
          for(const auto c:field_text) {
            if((c < ' ') || (c == '"') || (c > '~') || (c == delimiter_)) return quote(field_text);
          }
        }
        return string_type(field_text);
      }

    public:

      /**
       * Clears the internal state of this composer,
       * so that a new data set (with `define_columns()`)
       * can be started.
       * @return basic_composer&
       */
      basic_composer& clear()
      {
        quote_cols_.clear();
        num_cols_ = 0;
        return *this;
      }

      /**
       * Defines the number of expected columns. Feeding later
       * with incorrect column count will throw an exception.
       * @param const size_t num_cols
       * @return basic_composer&
       * @throws std::runtime_error
       */
      basic_composer& define_columns(const size_t num_cols)
      { return define_columns(num_cols, std::array<size_t,0>{}); }

      /**
       * Defines the number of expected columns, and which of the
       * columns (indexing 1 to N, not 0 to N-1) of these columns
       * must be quoted. Other columns are automatically quoted
       * when feeding new data row, if the corresponding fields
       * need to be escaped.
       * Feeding with incorrect column count will throw an exception.
       * @tparam typename ForceQuoteContainer
       * @param const size_t num_cols
       * @return basic_composer&
       * @throws std::runtime_error
       */
      template<typename ForceQuoteContainer>
      basic_composer& define_columns(size_t num_cols, const ForceQuoteContainer& forced_quote_indices)
      {
        if(num_cols_ > 0) throw std::runtime_error("CSV columns are already defined.");
        if(num_cols <= 0) throw std::runtime_error("CSV column count definition is invalid.");
        num_cols_ = num_cols;
        using sz_type = typename quote_cols_type::size_type;
        quote_cols_.resize(sz_type(num_cols));
        for(auto& e:quote_cols_) e = 0;
        for(const auto& i:forced_quote_indices) {
          if((i<=0) || (size_t(i)>num_cols_)) throw std::runtime_error("CSV forced quote index out of range (use 1 to N).");
          quote_cols_[sz_type(i-1)] = 1;
        }
        return *this;
      }

      /**
       * Compose one data row, apply quoting as defined (or escaping
       * if needed), join fields with the `delimiter()`, and add the
       * `newline()`. Then, this composed line passed to the `on_row`
       * function given while constructing this composed instance.
       * If the container field count does not match the number of
       * columns specified in `define_columns()`, an exception will be
       * thrown.
       * @tparam typename StringViewContainer: @concept forward-iterable
       * @param const StringViewContainer& fields
       * @return basic_composer&
       * @throws std::runtime_error
       */
      template<typename StringViewContainer>
      basic_composer& feed(const StringViewContainer& fields)
      {
        size_t i = 0;
        auto line = string_type();
        for(const auto& field:fields) {
          if(i > 0) line += delimiter();
          if(i >= num_cols_) throw std::runtime_error("CSV row feed exceeds the number of defined columns.");
          line += (quote_cols_[i]) ? (quote(field)) : (escape(field));
          ++i;
        }
        if(i != num_cols_) throw std::runtime_error("CSV row feed is missing columns.");
        line += newline();
        row_handler_(line);
        return *this;
      }

    private:

      const row_handler_type row_handler_;  // Function invoked for each CSV line composed.
      const char_type delimiter_;           // The CSV separator character.
      const string_type newline_;           // The newline sequence

      quote_cols_type quote_cols_;          // Forced-quoted column definitions (1: must quote, 0: only if needed). Intentionally not bool.
      size_t num_cols_;                     // Expected number of pushed columns.
    };

  }

  /**
   * CSV composer default specialization.
   */
  using csv_composer = detail::basic_composer<std::string, std::vector<int>>; // NOLINT Default: byte string, 1MB file reading buffer cap.

}}


// NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
// clang-format on
#endif
