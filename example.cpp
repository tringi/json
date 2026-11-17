#include "json.hpp"
#include <span>

using namespace std::literals;

// intentionally malformed JSON showcasing supported extensions and resiliency
auto text = R"(
{
     "pi" : 3.1415,
     "happy" : true,
     "name" : "Niels",
     "nothing" : null, // comment
     "answers" : {
         "everything": 42
     },
     /a : 2,
     \e : 1,
     "object" 2nd_key : ext_value {
         "currency": "USD",
         "value" : 42.99 { and some subobject }
     },
     "list" : [12, 13, a }, 14], // bug intentional
     "object" : {
         "curren\x63y": "CZK", // escaped 'c'
         "value" : 1000.00,
         value : {
            inner2 : {
               inner3 : innerValue,
            },
            /* comment,
               comment */z:3,
         },
         value : a;
     }
 }
// aa)"sv;

void print (const std::span <const ext::json::reference> & refs) {
    std::printf ("[%zu]: ", refs.size ());
    if (!refs.empty ()) {
        std::printf ("'%.*s'", (int) refs.front ().token.size (), refs.front ().token.data ());
        for (auto & r : refs.subspan (1)) {
            std::printf (", '%.*s'", (int) r.token.size (), r.token.data ());
        }
    }
    std::printf ("\n");
}

void raw (std::string_view s) {
    std::printf ("%02X", (unsigned char) s.front ());
    s.remove_prefix (1);
    for (auto c : s) {
        std::printf (" %02X", (unsigned char) c);
    }
}

int main () {

    // simplest example accessing single value with 'get'
    // note that ""sv is internally substituted for 'ext::json::rule::empty' here for convenience
    //  - while the later matches empty key, the former would require single empty token

    if (auto r = ext::json::get (text, { ""sv, "object"sv, "value"sv })) {
        printf ("GET object:value: '%.*s'\n\n", (int) r.token.size (), r.token.data ());
    } else {
        printf ("GET object:value NOT FOUND\n\n");
    }

    // simple access to a first whole object

    if (auto r = ext::json::find (text, { ""sv, "object"sv })) {
        printf ("FIND object:\n");
        printf ("KEY"); print (r.key);
        printf ("VAL"); print (r.value);
        printf ("ROW: %zu\n", r.line);
        printf ("BODY: '%.*s'\n\n", (int) r.body.size (), r.body.data ());
    } else {
        printf ("TEST object NOT FOUND\n\n");
    }

    // query with a full filter

    {
        std::vector <ext::json::filter> f;
        f.resize (3);

        f [0].rules.resize (1);
        f [0].rules [0].tokens.push_back (ext::json::rule::empty);
        f [1].rules.resize (1);
        f [1].rules [0].tokens.push_back ("object"sv);
        f [2].rules.resize (1);
        f [2].rules [0].tokens.push_back ("value"sv);

        if (auto r = ext::json::query1 (text, f)) {
            printf ("QUERY1 object:\n");
            printf ("KEY"); print (r.key);
            printf ("VAL"); print (r.value);
            printf ("ROW: %zu\n", r.line);
            printf ("BODY: '%.*s'\n\n", (int) r.body.size (), r.body.data ());
        } else {
            printf ("QUERY1 object NOT FOUND\n\n");
        }
    }

    // query set of items
    //  - simplified filter construction: 'filter' can construct from 'string_view'

    {
        std::vector <ext::json::filter> f;
        f.push_back (ext::json::rule::empty); // first element of proper JSON is empty
        f.push_back ("list"sv);               // querying set of "list" elements
        f.push_back (ext::json::rule::empty); // array items don't have keys

        if (auto set = ext::json::query (text, { ext::json::rule::empty, "list"sv, ext::json::rule::empty })) {
            printf ("QUERY list items found:\n");

            for (auto & o : set.objects) {
                printf ("ROW: %zu\n", o.line);
                printf ("KEY"); print (o.key);
                printf ("VAL"); print (o.value);
                //printf ("BODY: '%.*s'\n\n", (int) o.body.size (), o.body.data ());

                for (auto & r : o.members) {
                    printf ("  ROW: %zu\n", r.line);
                    printf ("    KEY: "); print (r.key);
                    printf ("    VAL: "); print (r.value);
                }
            }
            printf ("\n");
        } else {
            printf ("QUERY list items NONE FOUND\n\n");
        }
    }

    // query "currency" and "value" in "object" objects from the sample JSON
    //  - example of explicit construction of filters

    {
        std::vector <ext::json::filter> f;
        f.resize (3);

        f [0].rules.resize (1);
        f [0].rules [0].tokens.push_back (ext::json::rule::empty);
        f [1].rules.resize (1);
        f [1].rules [0].tokens.push_back ("object"sv);
        f [2].rules.resize (2);
        f [2].rules [0].tokens.push_back ("currency"sv);
        f [2].rules [1].tokens.push_back ("value"sv);

        if (auto set = ext::json::query (text, f)) {
            printf ("QUERY currency&value found:\n");

            for (auto & o : set.objects) {
                printf ("ROW: %zu\n", o.line);
                printf ("KEY"); print (o.key);
                printf ("VAL"); print (o.value);
                // printf ("BODY: '%.*s'\n\n", (int) o.body.size (), o.body.data ());

                for (auto & r : o.members) {
                    printf ("  ROW: %zu\n", r.line);
                    printf ("    KEY: "); print (r.key);
                    printf ("    VAL: "); print (r.value);
                }
            }
            printf ("\n");
        } else {
            printf ("QUERY currency&value NONE FOUND\n\n");
        }
    }

    // unescape

    auto s1 = "curren\\x63y"sv;
    printf ("UNESCAPE: '%.*s' to '%s'\n",
            (int) s1.size (), s1.data (),
            ext::json::unescape (s1).c_str ());

    auto s2 = "\\uD83D\\uDE80"sv;
    auto u2 = ext::json::unescape (s2);
    auto rocket = "🚀"sv;
    
    printf ("UNESCAPE: ");
    printf ("'%.*s' to \"", (int) s2.size (), s2.data ()); raw (u2);
    printf ("\", is it rocket \""); raw (rocket);
    printf ("\" ??? %s\n", u2 == rocket ? "true" : "false");

    return 0;
}
