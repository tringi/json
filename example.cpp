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
    bool first = true;
    std::printf ("[%zu]: ", refs.size ());
    for (auto & r : refs) {
        if (!first) {
            std::printf (", ");
        }
        std::printf ("'%.*s'", (int) r.token.size (), r.token.data ());
        first = false;
    }
    std::printf ("\n");
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
        ext::json::filter f2;
        ext::json::filter f1;
        ext::json::filter f0;

        f0.rules.resize (1);
        f0.rules [0].tokens.push_back (ext::json::rule::empty);
        f1.rules.resize (1);
        f1.rules [0].tokens.push_back ("object"sv);
        f2.rules.resize (1);
        f2.rules [0].tokens.push_back ("value"sv);
        f0.next = &f1;
        f1.next = &f2;

        if (auto r = ext::json::query1 (text, &f0)) {
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

    {
        ext::json::filter f2;
        ext::json::filter f1;
        ext::json::filter f0;

        f0.rules.resize (1);
        f0.rules [0].tokens.push_back (ext::json::rule::empty); // first element of proper JSON is empty
        f1.rules.resize (1);
        f1.rules [0].tokens.push_back ("list"sv); // querying set of "list" elements
        f2.rules.resize (1);
        f2.rules [0].tokens.push_back (ext::json::rule::empty); // array items don't have keys
        f0.next = &f1;
        f1.next = &f2;

        if (auto set = ext::json::query (text, &f0)) {
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

    {
        ext::json::filter f2;
        ext::json::filter f1;
        ext::json::filter f0;

        f0.rules.resize (1);
        f0.rules [0].tokens.push_back (ext::json::rule::empty);
        f1.rules.resize (1);
        f1.rules [0].tokens.push_back ("object"sv);
        f2.rules.resize (2);
        f2.rules [0].tokens.push_back ("currency"sv);
        f2.rules [1].tokens.push_back ("value"sv);
        f0.next = &f1;
        f1.next = &f2;

        if (auto set = ext::json::query (text, &f0)) {
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

    return 0;
}
