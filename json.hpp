#ifndef EXT_JSON_HPP
#define EXT_JSON_HPP

#include <string_view>
#include <stdexcept>
#include <vector>

// ext::json
//  - permissive JSON text parser; extracts string_views according to provided filter
//  - does ZERO processing (no unescaping or other translations)
//  - differences from standard JSON:
//     - key and value are vectors, not just single identifier/string
//     - each member or array element is considered a full object with key, value and a body
//     - escaping \v and \' are supported

namespace ext::json {

    // reference
    //  - references a token retrieved from the source JSON text
    //
    struct reference {
        std::string_view token;          // key/value token, or a full text of object body
        char             quoted = false; // a character the value was quoted by, or 0 if not
        std::size_t      line = 0;       // text input line at which this reference was found

        explicit operator bool () const noexcept {
            return this->token.size () != 0
                || this->quoted != false;
        }
    };

    // result
    //  - result of a query describing a JSON member
    //  - extensions:
    //     - 'key' and 'value' as a vector of more than one token, JSON can have only one
    //     - 'body' because JSON members can't have one
    //
    struct result {
        std::vector <reference> key;
        std::vector <reference> value;
        std::string_view        body;
        std::size_t             line = 0;

        explicit operator bool () const noexcept {
            return !this->key.empty ()
                || !this->value.empty ()
                || !this->body.empty ();
        }
    };

    // results
    //  - full two-level 'query' results
    //  - TODO: naming?
    //
    struct results {

        // set
        //  - members 
        //
        struct set : result {
            std::vector <result> members;
        };

        // objects
        //  - objects or array items (are 'values' too in JSON nomenclature)
        //
        std::vector <set> objects;

        explicit operator bool () const noexcept {
            return !this->objects.empty ();
        }
    };

    // rule
    //  - a rule for sequence of tokens to be considered
    //
    struct rule {
        std::vector <std::string_view> tokens;

        // trailing
        //  - match member to this rule, even if there are trailing key tokens
        //  - e.g. rule "aa bb" will match member key "aa bb cc"
        //
        bool trailing = true; 

        // TODO: wildcards?

    public:

        // empty
        //  - special rule; use single 'ext::json::rule::empty' for queries to match empty key
        //  - empty means zero tokens, and due to the parser design:
        //     - first level of standard valid JSON is considered 'empty', or rather anything starting with '{'
        //     - plain array values have 'empty' key, e.g.: [a,b,c] are three results with empty 'key' and one 'value'
        //
        static const std::string_view empty;

        // any
        //  - special rule; use single 'ext::json::rule::any' for queries to match every member content
        //
        static const std::string_view any;
    };

    // filter
    //  - set of 'rules' in a singly linked list, one per each nesting level in the JSON text
    //
    struct filter {

        // rules
        //  - multiple rules for keys
        //  - at least one of them must match for a member to pass the filter
        //
        std::vector <rule> rules;

        // next
        //  - filters are chained via this pointer
        //
        const filter * next = nullptr;
    };

    // ext::json exceptions
    //  - all functions may throw one of these
    //  - all functions may throw also std::bad_alloc

    struct recursion_exceeded : std::invalid_argument {
        std::size_t depth;
        recursion_exceeded (std::size_t depth)
            : std::invalid_argument ("json text nesting exceeded limit")
            , depth (depth) {};
    };

    // get
    //  - simplest version
    //  - returns single reference to first value found by the list of simple keys
    //  - get (text, { ""sv, "object"sv, "value"sv }) returns reference to 42.99
    //     - the empty string_view is replaced by ext::json::rule::empty
    //
    reference get (std::string_view json, std::initializer_list <std::string_view> keys);

    // find
    //  - like get, but returns full 'result' 
    //     - meaning key and value vectors, and optional body references
    //
    result find (std::string_view json, std::initializer_list <std::string_view> keys);

    // query1
    //  - full query for a single result
    //  - NOTE: no empty string_view substitution is performed
    //
    result query1 (std::string_view json, const filter * filter);

    // query
    //  - complete query for potentially multiple sets of members
    //
    results query (std::string_view json, const filter * filter);

    // unescape
    //  - replaces all escape sequences with corresponding characters
    //  - longer \x and \u sequences are turned into corresponding UTF-8 sequences
    //
    std::string unescape (std::string_view);
}

#endif
