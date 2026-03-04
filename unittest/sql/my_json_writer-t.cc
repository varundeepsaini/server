/*
   Copyright (c) 2021, MariaDB Corporation.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA */

#include <my_global.h>
#include <my_pthread.h>
#include <my_sys.h>
#include <stdio.h>
#include <tap.h>

/*
  Unit tests for class Json_writer.
*/

struct TABLE;
class Json_writer;


/* Several fake objects */
class Opt_trace
{
public:
  void enable_tracing_if_required() {}
  void disable_tracing_if_required() {}
  Json_writer *get_current_json() { return nullptr; }
};

class THD
{
public:
  Opt_trace opt_trace;
};

constexpr uint FAKE_SELECT_LEX_ID= UINT_MAX;

#define sql_print_error printf

#define JSON_WRITER_UNIT_TEST
#include "../sql/my_json_writer.h"
#include "../sql/my_json_writer.cc"

static bool json_output_eq(Json_writer &w, const char *expected)
{
  const String *s= w.output.get_string();
  size_t exp_len= strlen(expected);
  if (s->length() == exp_len && memcmp(s->ptr(), expected, exp_len) == 0)
    return true;
  diag("  Expected (%d bytes): [%s]", (int) exp_len, expected);
  diag("  Actual   (%d bytes): [%.*s]",
       (int) s->length(), (int) s->length(), s->ptr());
  return false;
}

int main(int args, char **argv)
{
  MY_INIT(argv[0]);

  plan(NO_PLAN);
  diag("Testing Json_writer checks");

  {
    Json_writer w;
    w.start_object();
    w.add_member("foo");
    w.end_object();
    ok(w.invalid_json, "Started a name but didn't add a value");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_ull(123);
    ok(w.invalid_json, "Unnamed value in an object");
  }

  {
    Json_writer w;
    w.start_array();
    w.add_member("bebebe").add_ull(345);
    ok(w.invalid_json, "Named member in array");
  }

  {
    Json_writer w;
    w.start_object();
    w.start_array();
    ok(w.invalid_json, "Unnamed array in an object");
  }

  {
    Json_writer w;
    w.start_object();
    w.start_object();
    ok(w.invalid_json, "Unnamed object in an object");
  }

  {
    Json_writer w;
    w.start_array();
    w.add_member("zzz");
    w.start_object();
    ok(w.invalid_json, "Named object in an array");
  }
  {
    Json_writer w;
    w.start_array();
    w.add_member("zzz");
    w.start_array();
    ok(w.invalid_json, "Named array in an array");
  }

  {
    Json_writer w;
    w.start_array();
    w.end_object();
    ok(w.invalid_json, "JSON object end of array");
  }

  {
    Json_writer w;
    w.start_object();
    w.end_array();
    ok(w.invalid_json, "JSON array end of object");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("name").add_ll(1);
    w.add_member("name").add_ll(2);
    w.end_object();
    ok(w.invalid_json, "JSON object member name collision");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("name").start_object();
    w.add_member("name").add_ll(2);
    ok(!w.invalid_json, "Valid JSON: nested object member name is the same");
  }

  diag("Testing Json_writer output");

  {
    Json_writer w;
    w.start_object();
    w.end_object();
    ok(json_output_eq(w, "{}"), "Empty object");
    ok(!w.invalid_json, "Empty object is valid");
  }

  {
    Json_writer w;
    w.start_array();
    w.end_array();
    ok(json_output_eq(w, "[]"), "Empty array");
    ok(!w.invalid_json, "Empty array is valid");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("key").add_str("hello");
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"key\": \"hello\"\n"
       "}"),
       "String value");
    ok(!w.invalid_json, "String value is valid");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("num").add_ll(42);
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"num\": 42\n"
       "}"),
       "Positive longlong value");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("num").add_ll(-100);
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"num\": -100\n"
       "}"),
       "Negative longlong value");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("num").add_ull(999);
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"num\": 999\n"
       "}"),
       "Unsigned longlong value");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("flag").add_bool(true);
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"flag\": true\n"
       "}"),
       "Bool true");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("flag").add_bool(false);
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"flag\": false\n"
       "}"),
       "Bool false");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("val").add_null();
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"val\": null\n"
       "}"),
       "Null value");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("pi").add_double(3.14);
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"pi\": 3.14\n"
       "}"),
       "Double value");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("a").add_str("hello");
    w.add_member("b").add_ll(42);
    w.add_member("c").add_bool(true);
    w.add_member("d").add_null();
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"a\": \"hello\",\n"
       "  \"b\": 42,\n"
       "  \"c\": true,\n"
       "  \"d\": null\n"
       "}"),
       "Object with multiple typed members");
    ok(!w.invalid_json, "Multiple members is valid");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("arr");
    w.start_array();
    w.add_str("a");
    w.add_str("b");
    w.add_str("c");
    w.end_array();
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"arr\": [\"a\", \"b\", \"c\"]\n"
       "}"),
       "Named string array: single-line format");
    ok(!w.invalid_json, "Named string array is valid");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("one");
    w.start_array();
    w.add_str("only");
    w.end_array();
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"one\": [\"only\"]\n"
       "}"),
       "Named array with single element");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("empty");
    w.start_array();
    w.end_array();
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"empty\": []\n"
       "}"),
       "Empty named array");
    ok(!w.invalid_json, "Empty named array is valid");
  }

  {
    Json_writer w;
    w.start_array();
    w.add_str("x");
    w.add_str("y");
    w.end_array();
    ok(json_output_eq(w,
       "[\n"
       "  \"x\",\n"
       "  \"y\"\n"
       "]"),
       "Root array: multi-line format");
    ok(!w.invalid_json, "Root array is valid");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("outer").start_object();
    w.add_member("inner").add_str("val");
    w.end_object();
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"outer\": {\n"
       "    \"inner\": \"val\"\n"
       "  }\n"
       "}"),
       "Nested objects");
    ok(!w.invalid_json, "Nested objects valid");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("data").start_object();
    w.add_member("name").add_str("test");
    w.add_member("count").add_ll(5);
    w.end_object();
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"data\": {\n"
       "    \"name\": \"test\",\n"
       "    \"count\": 5\n"
       "  }\n"
       "}"),
       "Nested object with multiple members");
  }

  {
    Json_writer w;
    w.start_array();
    w.start_object();
    w.add_member("id").add_ll(1);
    w.end_object();
    w.start_object();
    w.add_member("id").add_ll(2);
    w.end_object();
    w.end_array();
    ok(json_output_eq(w,
       "[\n"
       "  {\n"
       "    \"id\": 1\n"
       "  },\n"
       "  {\n"
       "    \"id\": 2\n"
       "  }\n"
       "]"),
       "Array of objects");
    ok(!w.invalid_json, "Array of objects valid");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("items");
    w.start_array();
    w.start_object();
    w.add_member("x").add_ll(10);
    w.end_object();
    w.start_object();
    w.add_member("x").add_ll(20);
    w.end_object();
    w.end_array();
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"items\": [\n"
       "    {\n"
       "      \"x\": 10\n"
       "    },\n"
       "    {\n"
       "      \"x\": 20\n"
       "    }\n"
       "  ]\n"
       "}"),
       "Object with array of objects");
    ok(!w.invalid_json, "Object with array of objects valid");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("l1").start_object();
    w.add_member("l2").start_object();
    w.add_member("l3").add_str("deep");
    w.end_object();
    w.end_object();
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"l1\": {\n"
       "    \"l2\": {\n"
       "      \"l3\": \"deep\"\n"
       "    }\n"
       "  }\n"
       "}"),
       "Three levels of nesting");
    ok(!w.invalid_json, "Deep nesting valid");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("name").add_str("test");
    w.add_member("enabled").add_bool(true);
    w.add_member("config").start_object();
    w.add_member("timeout").add_ll(30);
    w.add_member("tags");
    w.start_array();
    w.add_str("fast");
    w.add_str("reliable");
    w.end_array();
    w.end_object();
    w.add_member("count").add_ll(0);
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"name\": \"test\",\n"
       "  \"enabled\": true,\n"
       "  \"config\": {\n"
       "    \"timeout\": 30,\n"
       "    \"tags\": [\"fast\", \"reliable\"]\n"
       "  },\n"
       "  \"count\": 0\n"
       "}"),
       "Complex mixed structure");
    ok(!w.invalid_json, "Complex structure valid");
  }

  diag("Testing RAII wrappers");

  {
    Json_writer w;
    {
      Json_writer_object obj(&w);
      w.add_member("key").add_str("val");
    }
    ok(json_output_eq(w,
       "{\n"
       "  \"key\": \"val\"\n"
       "}"),
       "Json_writer_object auto-closes on destruction");
    ok(!w.invalid_json, "RAII object valid");
  }

  {
    Json_writer w;
    {
      Json_writer_object obj(&w);
      obj.add("name", "test");
      obj.add("count", (longlong) 7);
      obj.add("flag", true);
    }
    ok(json_output_eq(w,
       "{\n"
       "  \"name\": \"test\",\n"
       "  \"count\": 7,\n"
       "  \"flag\": true\n"
       "}"),
       "Json_writer_object fluent add()");
    ok(!w.invalid_json, "Fluent add valid");
  }

  {
    Json_writer w;
    Json_writer_object obj(&w);
    obj.add("a", (longlong) 1);
    obj.end();
    ok(json_output_eq(w,
       "{\n"
       "  \"a\": 1\n"
       "}"),
       "Json_writer_object explicit end()");
    ok(!w.invalid_json, "Explicit end valid");
  }

  {
    Json_writer w;
    {
      Json_writer_array arr(&w);
      arr.add("foo");
      arr.add("bar");
    }
    ok(json_output_eq(w,
       "[\n"
       "  \"foo\",\n"
       "  \"bar\"\n"
       "]"),
       "Json_writer_array auto-closes on destruction");
    ok(!w.invalid_json, "RAII array valid");
  }

  {
    Json_writer w;
    {
      Json_writer_object outer(&w);
      outer.add("level", "outer");
      {
        Json_writer_object inner(&w, "nested");
        inner.add("level", "inner");
      }
    }
    ok(json_output_eq(w,
       "{\n"
       "  \"level\": \"outer\",\n"
       "  \"nested\": {\n"
       "    \"level\": \"inner\"\n"
       "  }\n"
       "}"),
       "Nested RAII Json_writer_objects");
    ok(!w.invalid_json, "Nested RAII valid");
  }

  {
    Json_writer w;
    {
      Json_writer_object obj(&w);
      obj.add("name", "test");
      {
        Json_writer_array arr(&w, "values");
        arr.add("a");
        arr.add("b");
      }
    }
    ok(json_output_eq(w,
       "{\n"
       "  \"name\": \"test\",\n"
       "  \"values\": [\"a\", \"b\"]\n"
       "}"),
       "RAII array inside RAII object");
    ok(!w.invalid_json, "RAII array in object valid");
  }

  {
    Json_writer_object obj((Json_writer*) nullptr);
    obj.add("key", "val");
    obj.add("num", (longlong) 5);
    ok(!obj.trace_started(), "NULL writer: trace not started");
  }
  {
    Json_writer_array arr((Json_writer*) nullptr);
    arr.add("val");
    arr.add((longlong) 5);
    ok(!arr.trace_started(), "NULL writer array: trace not started");
  }

  diag("Testing String_with_limit");

  {
    String_with_limit sl;
    sl.append("hello");
    sl.append(" world");
    ok(sl.length() == 11, "Normal append: length == 11");
    ok(sl.get_truncated_bytes() == 0, "Normal append: no truncation");
    const String *s= sl.get_string();
    ok(s->length() == 11 && memcmp(s->ptr(), "hello world", 11) == 0,
       "Normal append: content correct");
  }

  {
    String_with_limit sl;
    sl.set_size_limit(5);
    sl.append("hello world");
    ok(sl.length() == 5, "Truncation: length capped at 5");
    ok(sl.get_truncated_bytes() == 6, "Truncation: 6 bytes truncated");
    const String *s= sl.get_string();
    ok(s->length() == 5 && memcmp(s->ptr(), "hello", 5) == 0,
       "Truncation: content is prefix");
  }

  {
    String_with_limit sl;
    sl.set_size_limit(5);
    sl.append("hi");
    sl.append(" there buddy");
    ok(sl.length() == 5, "Multi-append truncation: length == 5");
    ok(sl.get_truncated_bytes() == 9, "Multi-append truncation: 9 truncated");
    const String *s= sl.get_string();
    ok(s->length() == 5 && memcmp(s->ptr(), "hi th", 5) == 0,
       "Multi-append truncation: correct partial content");
  }

  {
    String_with_limit sl;
    sl.set_size_limit(3);
    sl.append("abc");
    sl.append("def");
    ok(sl.length() == 3, "At-limit overflow: length == 3");
    ok(sl.get_truncated_bytes() == 3, "At-limit overflow: 3 truncated");
  }

  {
    String_with_limit sl;
    sl.set_size_limit(3);
    sl.append('a');
    sl.append('b');
    sl.append('c');
    sl.append('d');
    ok(sl.length() == 3, "Char append truncation: length == 3");
    ok(sl.get_truncated_bytes() == 1, "Char append truncation: 1 truncated");
  }

  diag("Testing Json_writer size limit");

  {
    Json_writer w;
    w.set_size_limit(10);
    w.start_object();
    w.add_member("longkey").add_str("longvalue");
    w.end_object();
    ok(w.output.length() == 10, "Size limit enforced on Json_writer");
    ok(w.get_truncated_bytes() > 0, "Truncated bytes reported");
  }

  diag("Testing add_size formatting");

  {
    Json_writer w;
    w.start_object();
    w.add_member("size").add_size(512);
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"size\": \"512\"\n"
       "}"),
       "add_size: bytes (< 1024)");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("size").add_size(2048);
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"size\": \"2Kb\"\n"
       "}"),
       "add_size: Kb value");
  }

  {
    Json_writer w;
    w.start_object();
    w.add_member("size").add_size((longlong) 32 * 1024 * 1024);
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"size\": \"32Mb\"\n"
       "}"),
       "add_size: Mb value");
  }

  diag("Testing add_str overloads");

  {
    Json_writer w;
    String s;
    s.append("hello", 5);
    w.start_object();
    w.add_member("msg").add_str(s);
    w.end_object();
    ok(json_output_eq(w,
       "{\n"
       "  \"msg\": \"hello\"\n"
       "}"),
       "add_str with String object");
  }

  diag("Done");

  my_end(MY_CHECK_ERROR);
  return exit_status();
}
