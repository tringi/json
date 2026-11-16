# ext::json
A simple permissive [JSON](https://www.json.org/json-en.html) parser.
Walks the input text and returns reference(s) to data matching the filter.
Supports several extensions and unescaping into UTF-8.

## Features

* No tree construction, just parsing
* Search value/member/objects according to a filter
* Returns views into the original input
* Function to unescape to UTF-8 (supports surrogate pairs)
* Transparently recovers from various syntax errors
* Extension: Multi-token keys and values
* Extension: Extra escape character: `\v`, `\e` and `\'`
* Extension: C style comments

## Unified syntax

`[key1 [key2 [...]] ":"] [value1 [value2 [...]]] ["{" | "["] [element ["," [element]]] ["}" | "]"] [","]`

* key - one or more tokens, separated from value by `:` character
* value - one or more tokens
* body - encapsulated by `{` and `}` or `[` and `]`, containing zero or more elements
* token - quoted or unquoted string
* element separator is `,` comma
* array values are produced as vector of elements with a single value token

## API

Structures:

* `ext::json::reference` refers to a token  
  *provides `string_view`, line number, and whether and how it is quoted*
* `ext::json::result` refers to an element  
  *provides vector of references to key tokens, value tokens and a `string_view` of the body*
* `ext::json::results` provides **TWO** levels of elements  
  *used to extract members from multiple objects and arrays; best to see example below*

Functions:

* `ext::json::get` - provides highest abstraction, find and returns single value (token)
* `ext::json::find` - finds a whole element: key(s), value(s), body and line number
* `ext::json::query1` - general query for a single element
* `ext::json::query` - general query for a multiple objects of multiple element
* `ext::json::unescape` - helper function for unescaping token to UTF-8 string

Exceptions:

* `std::bad_alloc` - the library uses vectors which may fail to grow
* `ext::json::recursion_exceeded` - maximum nesting depth of objects or arrays in input was exceeded

## Examples

TODO

## License

None. Use freely. Attribution appreciated but not required.
