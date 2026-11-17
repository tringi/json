#include "json.hpp"
//#include <unordered_map>

#ifdef _MSC_VER
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE
#endif

using namespace ext::json;
using namespace std::literals;

const std::string_view ext::json::rule::empty ("e");
const std::string_view ext::json::rule::any ("a");

static constexpr std::string_view escapee = "\"\\nrtbf/0"sv         "ve\'"sv; // the second half are this library extensions
static constexpr std::string_view escaped = "\"\\\n\r\t\b\f/\0"sv   "\v\x1B\'"sv;
static_assert (escapee.size () == escaped.size ());

namespace {
    static constexpr auto quotes = "\"\'"sv;
    static constexpr auto nontokens = ",:[]{}/"sv;
    static constexpr auto terminators = " \r\n\t,:[]{}/"sv; // JSON punctuators, whitespace and comment start slash
    static constexpr auto whitespace = " \r\n\t"sv;

    char32_t advance_by_character (std::string_view &);

    // ext::json context
    //  - parsing and tokenizing data and results collection
    //
    class context {
        std::size_t         offset = 0;
        std::vector <bool>  nesting; // false for object, true for array
        std::vector <reference> tokens;
        // std::unordered_map <char *, bool> opt;

        const std::string_view       text;
        const std::vector <filter> & filters;

    public: // out
        results results;
        result  result;

    public: // options

        // single_result
        //  - if true process will bail after first matching result is found
        //
        bool single_result = false;

        // max_nesting_depth
        //  - parser will fail on recursion into subobjects/arrays this deep
        //  - maximum: stack size remaining / ~256
        //
        std::size_t max_nesting_depth = 1024;

    public:
        context (std::string_view text, const std::vector <filter> & filters)
            : text (text)
            , filters (filters) { // 

            this->nesting.reserve (64); // 8 bytes
            this->tokens.reserve (3); // valid standard JSON can't have more than 3
        }

        bool process (bool skip) {
            tokens.clear ();
            while (offset != text.length ()) {

                this->skip_whitespace ();
                this->consume_tokens (skip);

                if (offset != text.length ()) {
                    switch (text [offset]) {

                        case '/':
                            if (offset + 1 < text.length ()) {
                                switch (text [offset + 1]) {
                                    case '/':
                                        this->skip_line_comment ();
                                        continue;
                                    case '*':
                                        this->skip_multiline_comment ();
                                        continue;
                                }
                            }
                            this->consume_character (skip);
                            break;

                        case ',':
                            if (this->finish_member ())
                                return true;

                            break;

                        case '[':
                        case '{': {
                            if (this->max_nesting_depth) {
                                if (this->nesting.size () + 1 == this->max_nesting_depth)
                                    throw recursion_exceeded (this->max_nesting_depth);
                            }

                            const auto f = at_last_filter_level ();
                            const auto s = skip || (f && !this->consider ());
                            const bool p = !s && !this->single_result && this->at_next_to_last_filter ();
                            const auto o = offset++;

                            if (p) {
                                this->results.objects.resize (this->results.objects.size () + 1);
                                this->results.objects.back ().key = this->result.key;
                                this->results.objects.back ().value = this->result.value;
                                this->results.objects.back ().line = this->result.line;
                            }

                            this->nesting.push_back (text [o] == '[');
                            if (this->process (s))
                                return true;

                            this->nesting.pop_back ();

                            if (!s) {
                                if (this->single_result) {
                                    result.body = text.substr (o, offset - o);
                                } else
                                if (p) {
                                    this->results.objects.back ().body = text.substr (o, offset - o);
                                }

                                if (f)
                                    if (this->collect ())
                                        return true;
                            }

                            // past this array or object, starting next object/member anyway, comma is optional

                            this->skip_whitespace ();
                            if (this->is_next (',')) {
                                ++offset;
                            }
                        } break;

                        case '}':
                        case ']':
                            if (!this->nesting.empty () && (this->nesting.back () == (text [offset] == ']'))) { // correct ending
                                return this->finish_member ();

                            } else {
                                this->consume_character (skip);
                                break;
                            }

                        // we don't know what to do with this, so just append it as token and continue consuming

                        default:
                            this->consume_character (skip);
                    }
                }
            }
            return false;
        }

        bool at_next_to_last_filter () const {
            return this->nesting.size () == this->filters.size () - 3;
        }
        bool at_last_filter_level () const {
            return this->nesting.size () == this->filters.size () - 1;
        }

        bool finish_member () {
            if (this->at_last_filter_level ()) {
                if (this->consider ()) {
                    if (this->collect ())
                        return true;
                }
            }

            ++offset;
            tokens.clear ();
            result.body = std::string_view ();
            return false;
        }

        bool consider () {

            // split sequence into key and value by first plain ':'

            auto split = std::find_if (tokens.begin (), tokens.end (),
                                       [] (auto ref) { return ref.quoted == false
                                       && ref.token == ":"sv; });

            result.key.assign (tokens.begin (), split);
            if (split != tokens.end ()) {
                result.value.assign (++split, tokens.end ());
            } else {
                result.key.swap (result.value); // no ':' means this is just a value
            }

            // match against filter rules

            for (auto & rule : this->filters.back ().rules) {
                if (match (rule))
                    return true;
            }
            return false;
        }

        // match
        //  - determines if current tokens in 'this->result.key' match specific 'rule' 
        //
        bool match (const ext::json::rule & rule) noexcept {
            if (!rule.trailing && (rule.tokens.size () < result.key.size ()))
                return false;

            // special cases
            //  - comparing unique pointers, not content

            if (rule.tokens.size () == 1) {
                if (rule.tokens [0].data () == rule::any.data ())
                    return true;

                if (rule.tokens [0].data () == rule::empty.data ())
                    return result.key.empty ();
            }

            if (result.key.size () < rule.tokens.size ())
                return false;
            
            auto n = std::min (rule.tokens.size (), result.key.size ());
            for (auto i = 0uZ; i != n; ++i) {
                if (!this->compare (rule.tokens [i], result.key [i].token))
                    return false;
            }
            return true;
        }

        bool collect () {
            if (this->single_result)
                return true;

            // TODO: testing for empty result is temporary workaround for a bug somewhere
            //       passing wrong items here or not cleaning up something somewhere

            if (!this->results.objects.empty () && this->result) {
                this->results.objects.back ().members.push_back (std::move (result));
                this->result.key.clear ();
                this->result.value.clear ();
                this->result.body = {};
            }
            return false;
        }

        bool skip_sequence_loop (std::size_t limit, auto predicate) noexcept {
            for (; offset < limit; ++offset) {
                if (!predicate (text [offset]))
                    return true;
            }
            return false;
        }

        bool skip_sequence (std::size_t limit, auto predicate) noexcept {
            return skip_sequence_loop (std::min (offset + limit, text.length ()), predicate);
        }
        bool skip_sequence (auto predicate) noexcept {
            return skip_sequence_loop (text.length (), predicate);
        }

        NOINLINE
        void skip_whitespace () noexcept {
            auto e = std::min (text.find_first_not_of (whitespace, offset), text.length ());
            result.line += std::count (text.begin () + offset, text.begin () + e, '\n');
            offset = e;
        }

        void skip_line_comment () noexcept {
            offset += 2;
            if (this->skip_sequence ([] (char c) { return c != '\n'; })) { // find nearest '\n'
                offset += 1;
            }
            result.line++;
        }

        void skip_multiline_comment () noexcept {
            offset += 2;

            auto end = text.find ("*/"sv, offset);
            if (end != std::string_view::npos) {
                result.line += std::count (text.begin () + offset, text.begin () + end, '\n');
                offset = end + 2;
            } else
                offset = text.length ();
        }

        static bool is_digit (char c) noexcept {
            return c >= '0' && c <= '9';
        }
        static bool is_hex_digit (char c) noexcept {
            if (c >= '0' && c <= '9') return true;
            if (c >= 'A' && c <= 'F') return true;
            if (c >= 'a' && c <= 'f') return true;
            return false;
        }

        void skip_escape_sequence () noexcept {
            ++offset;
            switch (text [offset++]) { // this automatically skips any \' \" \n \t and so
                case '0': break;
                case 'x': this->skip_sequence (2, is_hex_digit); break;
                case 'u': this->skip_sequence (4, is_hex_digit); break;

                default:
                    if (is_digit (text [offset])) {
                        this->skip_sequence (is_digit); // octal?
                    }
            }
        }

        char is_next (char c) const noexcept {
            if (offset < text.length ()) {
                if (text [offset] == c)
                    return c;
            }
            return '\0';
        }

        static constexpr char is_one_of (char c, std::string_view characters) noexcept {
            auto i = characters.find (c);
            if (i != std::string_view::npos) {
                return characters [i];
            } else
                return '\0';
        }

        char is_next_one_of (std::string_view characters) const noexcept {
            if (offset < text.length ())
                return is_one_of (text [offset], characters);
            else
                return '\0';
        }

        void consume_token (bool skip) noexcept {
            auto q = this->is_next_one_of (quotes);
            auto a = (q != '\0') ? 1 : 0;

            offset += a;

            auto o = offset;
            this->skip_to_ending (a ? std::string_view (&q, 1) : terminators);

            if (!skip) {
                tokens.emplace_back (text.substr (o, offset - o), q, this->result.line);
            }
            offset += a;
        }

        NOINLINE
        void consume_tokens (bool skip) {
            while (offset < text.length () && !is_next_one_of (nontokens)) {
                this->consume_token (skip);
                this->skip_whitespace ();
            }
        }

        void consume_character (bool skip) {
            if (!skip) {
                tokens.emplace_back (text.substr (offset, 1), false, this->result.line);
            }
            ++offset;
        }

        void skip_to_ending (std::string_view endings) noexcept {
            for (; offset < text.length (); ++offset) {
                if (text [offset] == '\\') {
                    this->skip_escape_sequence ();
                }
                if (is_one_of (text [offset], endings))
                    break;
            }
        }

        bool compare (std::string_view a, std::string_view b) {
            if (a == b)
                return true;

            // TODO: cache a.data () and b.data () pointers in this 'context' and a result of .contains('\\')
            //       in order to skip this loop early if not needed

            if (a.contains ('\\') || b.contains ('\\')) {
                while (!a.empty ()
                    && !b.empty ()
                    && advance_by_character (a) == advance_by_character (b))
                    ;
                return a.empty () && b.empty ();
            } else
                return false;
        }
    };

    bool char_to_hex (char c, std::uint32_t * number) noexcept {
        if (c >= '0' && c <= '9') { *number = c - '0'; return true; }
        if (c >= 'A' && c <= 'F') { *number = c - 'A' + 10; return true; }
        if (c >= 'a' && c <= 'f') { *number = c - 'a' + 10; return true; }
        return false;
    }

    // consume_hexadecimal_number
    //  - reads 'max' hexadecimal characters from 's' and converts them to a number
    //  - advances 's' by the number of characters consumed
    //  - returns 0 on failure
    //
    std::uint32_t consume_hexadecimal_number (std::string_view & s, std::size_t max) noexcept {
        std::uint32_t r = 0;
        std::uint32_t n = 0;
        std::size_t i = 0;

        while ((i != max) && (i < s.length ()) && char_to_hex (s [i], &n)) {
            r <<= 4;
            r += n;
            ++i;
        }
        s.remove_prefix (i);
        return r;
    }

    std::size_t count_escapes (const std::string_view & s) noexcept {
        std::size_t n = 0;
        std::size_t i = std::string_view::npos;

        while ((i = s.find_first_of (escapee, i + 1)) != std::string_view::npos) {
            if ((i > 0) && (s [i - 1] == '\\')) {
                ++n;
            }
        }
        return n;
    }

    std::size_t count_escapes (const std::string_view & s, char c) noexcept {
        std::size_t n = 0;
        std::size_t i = std::string_view::npos;

        while ((i = s.find (c, i + 1)) != std::string_view::npos) {
            if ((i > 0) && (s [i - 1] == '\\')) {
                ++n;
            }
        }
        return n;
    }

    void insert_as_utf8 (std::uint32_t c, std::string & s) {
        if (c <= 0x7F) {
            s.push_back ((char) c);
        } else
        if (c <= 0x7FF) {
            s.push_back (0xC0 | ((c >> 6) & 0x1F));
            s.push_back (0x80 | (c & 0x3F));
        } else
        if (c <= 0xFFFF) {
            s.push_back (0xE0 | ((c >> 12) & 0x0F));
            s.push_back (0x80 | ((c >> 6) & 0x3F));
            s.push_back (0x80 | (c & 0x3F));
        } else
        if (c <= 0x10FFFF) {
            s.push_back (0xF0 | ((c >> 18) & 0x07));
            s.push_back (0x80 | ((c >> 12) & 0x3F));
            s.push_back (0x80 | ((c >> 6) & 0x3F));
            s.push_back (0x80 | (c & 0x3F));
        }
    }

    char32_t advance_by_character (std::string_view & s) {
        if (s.starts_with ('\\')) {
            s.remove_prefix (1);

            if (!s.empty ()) {
                switch (s.front ()) {
                    case 'u': s.remove_prefix (1); return consume_hexadecimal_number (s, 4);
                    case 'x': s.remove_prefix (1); return consume_hexadecimal_number (s, 2);

                    default:
                        auto i = escapee.find (s.front ());
                        if (i != std::string_view::npos) {
                            s.remove_prefix (1);
                            return escaped [i];
                        }
                }
            }
            return '\\';
        } else {
            char c = s.front ();
            s.remove_prefix (1);
            return (unsigned char) c;
        }
    }

}

ext::json::results
ext::json::query (std::string_view text, const std::vector <filter> & filters) {
    context c (text, filters);
    c.process (false);
    return c.results;
}

ext::json::result
ext::json::query1 (std::string_view text, const std::vector <filter> & filters) {
    context c (text, filters);
    c.single_result = true;
    c.process (false);
    return c.result;
}

ext::json::result
ext::json::find (std::string_view text, std::initializer_list <std::string_view> keys) {

    std::vector <filter> f;
    f.resize (keys.size ());

    auto i = 0uZ;
    for (auto key : keys) {
        f [i].rules.resize (1);
        f [i].rules [0].tokens.push_back (key.empty () ? ext::json::rule::empty : key);
        ++i;
    }

    return ext::json::query1 (text, f);
}

ext::json::reference
ext::json::get (std::string_view text, std::initializer_list <std::string_view> keys) {
    auto r = ext::json::find (text, keys);
    if (!r.value.empty ()) {
        return r.value.front ();
    } else
        return {};
}

std::string ext::json::unescape (std::string_view view) {
    std::string r;

    if (auto n = view.size ()) {

        // reserve rough estimate of final size
        //  - assume user escapes bigger characters, not ASCII

        n -= count_escapes (view);
        n -= 2 * count_escapes (view, 'x'); // "\x41" => "A" (4 => 1 or 2)
        n -= 3 * count_escapes (view, 'u'); // "\x0041" => "A" (6 => 1..3)

        // min to fix possible underflow on invalid escapes
        r.reserve (std::min (n, view.size ()));
    }

    while (!view.empty ()) {
        auto c = advance_by_character (view);

        // surrogate pairs
        //  - invalid surrogate pairs are propagated

        if (c >= 0xD800 && c <= 0xDBFF) {
            if (!view.empty ()) {

                auto c2 = advance_by_character (view);
                if (c2 >= 0xDC00 && c2 <= 0xDFFF) {
                    c = 0x10000 + ((c & 0x03FF) << 10) + (c2 & 0x03FF);
                    
                } else {
                    insert_as_utf8 (c, r);
                    c = c2;
                }
            }
        }

        insert_as_utf8 (c, r);
    }
    return r;
}
