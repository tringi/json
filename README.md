# ext::json
A simple permissive [JSON](https://www.json.org/json-en.html) parser. Pure C++.
Walks the input text and returns reference(s) to data matching the filter, think XPath but dumber.
Supports several extensions and unescaping into UTF-8.

## Features

* No tree construction, just parsing
* Searches value/member/objects according to a filter
* Returns views into the original input, no conversions
* Exrea function to unescape to UTF-8 (supports surrogate pairs)
* Transparently recovers from various syntax errors
* Extension: Multi-token keys and values
* Extension: Extra escape characters: `\v`, `\e` and `\'`
* Extension: C/C++ style comments

## Unified syntax

Members are also objects:

`[key1 [key2 [...]] ":"] [value1 [value2 [...]]] ["{" | "["] [member ["," [member]]] ["}" | "]"] [","]`

* key - one or more tokens, separated from value by `:` character
* value - one or more tokens
* body - encapsulated by `{` and `}` or `[` and `]`, containing zero or more members
* token - quoted or unquoted string
* member/object separator is `,` comma
* array values are produced as vector of members with a single value token

## API

Structures:

* `ext::json::reference` refers to a token  
  *provides `string_view`, line number, and whether and how it is quoted*
* `ext::json::result` refers to a member/object  
  *provides vector of references to key tokens, value tokens and a `string_view` of the body*
* `ext::json::results` provides **TWO** levels of objects  
  *used to extract members from multiple objects and arrays; best to see example below*

Functions:

* `ext::json::get` - provides highest abstraction, find and returns single value (token)
* `ext::json::find` - finds a whole member: key(s), value(s), body content and line number
* `ext::json::query1` - general query for a single object
* `ext::json::query` - general query for a multiple objects of a multiple members
* `ext::json::unescape` - helper function for unescaping token to UTF-8 string

Exceptions:

* `std::bad_alloc` - the library uses vectors which may fail to grow
* `ext::json::recursion_exceeded` - maximum nesting depth of objects or arrays in input was exceeded

## Examples

    auto text = R"(
    {
        "object" : {
            "currency": "CZK",
            "value" : 42.00,
        }
    }
    )"sv;

    if (auto r = ext::json::get (text, { ""sv, "object"sv, "value"sv })) {
        printf ("'%.*s' on LINE %zu\n", (int) r.token.size (), r.token.data (), r.line);
    }

Expected output is:

    '42.00' on LINE 5

## Performance

It's probably the slowest JSON parser, because of constant allocations in token vectors.

## License

None. Use freely. Attribution appreciated but not required.
