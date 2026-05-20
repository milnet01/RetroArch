/* Copyright  (C) 2021 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (test_stdstring.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <check.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include <string/stdstring.h>
#include <encodings/utf.h>

#define SUITE_NAME "stdstring"

START_TEST (test_string_filter)
{
   char test1[] = "foo bar some string";
   char test2[] = "";
   string_remove_all_chars(test1, 's');
   string_remove_all_chars(test2, '0');
   ck_assert_str_eq(test1, "foo bar ome tring");
   ck_assert_str_eq(test2, "");
}
END_TEST

START_TEST (test_string_replace)
{
   char test1[] = "foo bar some string";
   string_replace_all_chars(test1, 's', 'S');
   ck_assert_str_eq(test1, "foo bar Some String");
}
END_TEST

START_TEST (test_string_case)
{
   char test1[] = "foo";
   char test2[] = "01foOo[]_";
   ck_assert_str_eq(string_to_upper(test1), "FOO");
   ck_assert_str_eq(string_to_upper(test2), "01FOOO[]_");
   ck_assert_str_eq(string_to_lower(test2), "01fooo[]_");
}
END_TEST

START_TEST (test_string_char_classify)
{
   ck_assert(ISSPACE(' '));
   ck_assert(ISSPACE('\n'));
   ck_assert(ISSPACE('\r'));
   ck_assert(ISSPACE('\t'));
   ck_assert(!ISSPACE('a'));

   ck_assert(ISALPHA('a'));
   ck_assert(ISALPHA('Z'));
   ck_assert(!ISALPHA('5'));

   ck_assert(ISALNUM('a'));
   ck_assert(ISALNUM('Z'));
   ck_assert(ISALNUM('5'));

   /* ISUALPHA / ISUALNUM also accept '_' (identifier classification). */
   ck_assert(ISUALPHA('a'));
   ck_assert(ISUALPHA('Z'));
   ck_assert(ISUALPHA('_'));
   ck_assert(!ISUALPHA('5'));
   ck_assert(!ISUALPHA(' '));

   ck_assert(ISUALNUM('a'));
   ck_assert(ISUALNUM('5'));
   ck_assert(ISUALNUM('_'));
   ck_assert(!ISUALNUM(' '));
   ck_assert(!ISUALNUM('['));

   ck_assert(IS_XDIGIT('0'));
   ck_assert(IS_XDIGIT('9'));
   ck_assert(IS_XDIGIT('a'));
   ck_assert(IS_XDIGIT('f'));
   ck_assert(IS_XDIGIT('A'));
   ck_assert(IS_XDIGIT('F'));
   ck_assert(!IS_XDIGIT('g'));
   ck_assert(!IS_XDIGIT('G'));
   ck_assert(!IS_XDIGIT(' '));

   ck_assert_int_eq(TOUPPER('a'), 'A');
   ck_assert_int_eq(TOUPPER('A'), 'A');
   ck_assert_int_eq(TOUPPER('5'), '5');
   ck_assert_int_eq(TOLOWER('A'), 'a');
   ck_assert_int_eq(TOLOWER('a'), 'a');
   ck_assert_int_eq(TOLOWER('5'), '5');
}
END_TEST

START_TEST (test_string_to_unsigned)
{
   ck_assert_uint_eq(3, string_to_unsigned("3"));
   ck_assert_uint_eq(2147483647, string_to_unsigned("2147483647"));
   ck_assert_uint_eq(0, string_to_unsigned("foo"));
   ck_assert_uint_eq(0, string_to_unsigned("-1"));
   ck_assert_uint_eq(0, string_to_unsigned(NULL));
}
END_TEST

START_TEST (test_string_hex_to_unsigned)
{
   ck_assert_uint_eq(10, string_hex_to_unsigned("0xa"));
   ck_assert_uint_eq(10, string_hex_to_unsigned("a"));
   ck_assert_uint_eq(255, string_hex_to_unsigned("FF"));
   ck_assert_uint_eq(255, string_hex_to_unsigned("0xff"));
   ck_assert_uint_eq(0, string_hex_to_unsigned("0xfzzf"));
   ck_assert_uint_eq(0, string_hex_to_unsigned("0x"));
   ck_assert_uint_eq(0, string_hex_to_unsigned("0xx"));
   ck_assert_uint_eq(0, string_hex_to_unsigned(NULL));
}
END_TEST

START_TEST (test_string_tokenizer)
{
   char *testinput = "@@1@@2@@3@@@@9@@@";
   char **ptr = &testinput;
   char *token = NULL;
   token = string_tokenize(ptr, "@@");
   ck_assert_ptr_nonnull(token);
   ck_assert_str_eq(token, "");
   free(token);
   token = string_tokenize(ptr, "@@");
   ck_assert_ptr_nonnull(token);
   ck_assert_str_eq(token, "1");
   free(token);
   token = string_tokenize(ptr, "@@");
   ck_assert_ptr_nonnull(token);
   ck_assert_str_eq(token, "2");
   free(token);
   token = string_tokenize(ptr, "@@");
   ck_assert_ptr_nonnull(token);
   ck_assert_str_eq(token, "3");
   free(token);
   token = string_tokenize(ptr, "@@");
   ck_assert_ptr_nonnull(token);
   ck_assert_str_eq(token, "");
   free(token);
   token = string_tokenize(ptr, "@@");
   ck_assert_ptr_nonnull(token);
   ck_assert_str_eq(token, "9");
   free(token);
   token = string_tokenize(ptr, "@@");
   ck_assert_ptr_nonnull(token);
   ck_assert_str_eq(token, "@");
   free(token);
   token = string_tokenize(ptr, "@@");
   ck_assert_ptr_null(token);
}
END_TEST

START_TEST (test_string_replacesubstr)
{
   /* string_replace_substring took 3 args until commit ca7e53e3ca
    * (2024-12-23) made the lengths explicit. */
   const char *in       = "foobaarhellowooorldtest";
   const char *pattern  = "oo";
   const char *replace  = "ooo";
   char *res = string_replace_substring(
         in,      strlen(in),
         pattern, strlen(pattern),
         replace, strlen(replace));
   ck_assert_ptr_nonnull(res);
   ck_assert_str_eq(res, "fooobaarhellowoooorldtest");
   free(res);
}
END_TEST

START_TEST (test_string_trim)
{
   char test1[] = "\t \t\nhey there \n \n";
   char test2[] = "\t \t\nhey there \n \n";
   char test3[] = "\t \t\nhey there \n \n";
   ck_assert_ptr_eq(string_trim_whitespace_left(test1),  (char*)test1);
   ck_assert_str_eq(test1, "hey there \n \n");
   ck_assert_ptr_eq(string_trim_whitespace_right(test2), (char*)test2);
   ck_assert_str_eq(test2, "\t \t\nhey there");
   ck_assert_ptr_eq(string_trim_whitespace(test3),       (char*)test3);
   ck_assert_str_eq(test3, "hey there");
}
END_TEST

START_TEST (test_string_comparison)
{
   /* string_is_equal / string_starts_with / string_ends_with —
    * the SUT's comparison API. Prior test exercised raw memcmp,
    * which is not a stdstring contract. */
   ck_assert(string_is_equal("foo", "foo"));
   ck_assert(!string_is_equal("foo", "bar"));
   ck_assert(!string_is_equal("foo", "foo2"));
   ck_assert(!string_is_equal(NULL, "foo"));
   ck_assert(!string_is_equal("foo", NULL));
   ck_assert(string_is_equal(NULL, NULL));

   ck_assert(string_is_equal_case_insensitive("Foo", "fOO"));
   ck_assert(!string_is_equal_case_insensitive("Foo", "Bar"));

   ck_assert(string_is_empty(""));
   ck_assert(string_is_empty(NULL));
   ck_assert(!string_is_empty("x"));

   ck_assert(string_starts_with("foobar", "foo"));
   ck_assert(!string_starts_with("foobar", "bar"));
   ck_assert(string_starts_with("foo", "foo"));
   ck_assert(!string_starts_with("foo", "foobar"));

   ck_assert(string_ends_with("foobar", "bar"));
   ck_assert(!string_ends_with("foobar", "foo"));
   ck_assert(string_ends_with("foo", "foo"));
   ck_assert(!string_ends_with("foo", "foobar"));

   ck_assert(string_starts_with_case_insensitive("FooBar", "foo"));
   ck_assert(!string_starts_with_case_insensitive("FooBar", "bar"));
}
END_TEST

START_TEST (test_word_wrap)
{
   const char *testtxt = (
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nam nec "
      "enim quis orci euismod efficitur at nec arcu. Vivamus imperdiet est "
      "feugiat massa rhoncus porttitor at vitae ante. Nunc a orci vel ipsum "
      "tempor posuere sed a lacus. Ut erat odio, ultrices vitae iaculis "
      "fringilla, iaculis ut eros.\nSed facilisis viverra lectus et "
      "ullamcorper. Aenean risus ex, ornare eget scelerisque ac, imperdiet eu "
      "ipsum. Morbi pellentesque erat metus, sit amet aliquet libero rutrum "
      "et. Integer non ullamcorper tellus.");
   const char *expected = (
      "Lorem ipsum dolor sit amet, consectetur\n"
      "adipiscing elit. Nam nec enim quis orci\n"
      "euismod efficitur at nec arcu. Vivamus\n"
      "imperdiet est feugiat massa rhoncus\n"
      "porttitor at vitae ante. Nunc a orci\n"
      "vel ipsum tempor posuere sed a lacus.\n"
      "Ut erat odio, ultrices vitae iaculis\n"
      "fringilla, iaculis ut eros.\n"
      "Sed facilisis viverra lectus et\n"
      "ullamcorper. "
      "Aenean risus ex, ornare eget scelerisque ac, imperdiet eu ipsum. Morbi "
      "pellentesque erat metus, sit amet aliquet libero rutrum et. Integer "
      "non ullamcorper tellus.");

   char output[1024];

   word_wrap(output, sizeof(output), testtxt, strlen(testtxt), 40, 100, 10);
   ck_assert_str_eq(output, expected);
}
END_TEST

START_TEST (test_strlcpy)
{
   char buf1[8];
   ck_assert_uint_eq(3, strlcpy(buf1, "foo", sizeof(buf1)));
   ck_assert_str_eq(buf1, "foo");
   ck_assert_uint_eq(11, strlcpy(buf1, "foo12345678", sizeof(buf1)));
   ck_assert_str_eq(buf1, "foo1234");
}
END_TEST

START_TEST (test_strlcat)
{
   char buf1[8];
   buf1[0] = 'f';
   buf1[1] = '\0';
   ck_assert_uint_eq(10, strlcat(buf1, "ooooooooo", sizeof(buf1)));
   ck_assert_str_eq(buf1, "foooooo");
   ck_assert_uint_eq(13, strlcat(buf1, "123456", sizeof(buf1)));
   ck_assert_str_eq(buf1, "foooooo");
}
END_TEST

START_TEST (test_strldup)
{
   char buf1[8] = "foo";
   char *tv1 = strldup(buf1, 16);
   char *tv2 = strldup(buf1, 2);
   ck_assert_ptr_ne(tv1, (char*)buf1);
   ck_assert_ptr_ne(tv2, (char*)buf1);
   ck_assert_uint_eq(strlen(tv2), 1);
   ck_assert_int_eq((unsigned char)tv2[0], (unsigned char)'f');
   ck_assert_int_eq((unsigned char)tv2[1], 0);
   free(tv1);
   free(tv2);
}
END_TEST

START_TEST (test_utf8_conv_utf32)
{
   size_t count;
   uint32_t output[12];
   const char test1[] = "aæ⠻จйγチℝ\xff";
   count = utf8_conv_utf32(output, 12, test1, strlen(test1));
   ck_assert_uint_eq(8, count);
   /* Guard against count mismatch indexing into uninitialised slots. */
   if (count != 8)
      return;
   ck_assert_uint_eq(97, output[0]);
   ck_assert_uint_eq(230, output[1]);
   ck_assert_uint_eq(10299, output[2]);
   ck_assert_uint_eq(3592, output[3]);
   ck_assert_uint_eq(1081, output[4]);
   ck_assert_uint_eq(947, output[5]);
   ck_assert_uint_eq(12481, output[6]);
   ck_assert_uint_eq(8477, output[7]);
}
END_TEST

START_TEST (test_utf8_util)
{
   const char *test1 = "aæ⠻จ𠀤";
   const char **tptr = &test1;
   ck_assert_uint_eq(utf8len(test1), 5);
   ck_assert_uint_eq(utf8len(NULL), 0);
   ck_assert_ptr_eq((void*)&test1[1 + 2 + 3], (void*)utf8skip(test1, 3));

   ck_assert_uint_eq(97, utf8_walk(tptr));
   ck_assert_uint_eq(230, utf8_walk(tptr));
   ck_assert_uint_eq(10299, utf8_walk(tptr));
   ck_assert_uint_eq(3592, utf8_walk(tptr));
   ck_assert_uint_eq(131108, utf8_walk(tptr));
}
END_TEST

START_TEST (test_utf16_conv)
{
   const uint16_t test1[] = {0x0061, 0x00e6, 0x283b, 0x0e08, 0xd840, 0xdc24};
   char out[64];
   size_t outlen = sizeof(out);
   ck_assert(utf16_conv_utf8((uint8_t*)out, &outlen, test1, sizeof(test1) / 2));
   ck_assert_uint_eq(outlen, 13);
   ck_assert_msg(!memcmp(out, "aæ⠻จ𠀤", 13),
         "utf16_conv_utf8: 13-byte output does not match expected UTF-8");
}
END_TEST

Suite *create_suite(void)
{
   Suite *s = suite_create(SUITE_NAME);

   TCase *tc_core = tcase_create("Core");
   tcase_add_test(tc_core, test_string_comparison);
   tcase_add_test(tc_core, test_string_to_unsigned);
   tcase_add_test(tc_core, test_string_hex_to_unsigned);
   tcase_add_test(tc_core, test_string_char_classify);
   tcase_add_test(tc_core, test_string_case);
   tcase_add_test(tc_core, test_string_filter);
   tcase_add_test(tc_core, test_string_replace);
   tcase_add_test(tc_core, test_string_tokenizer);
   tcase_add_test(tc_core, test_string_trim);
   tcase_add_test(tc_core, test_string_replacesubstr);
   tcase_add_test(tc_core, test_word_wrap);
   tcase_add_test(tc_core, test_strlcpy);
   tcase_add_test(tc_core, test_strlcat);
   tcase_add_test(tc_core, test_strldup);
   tcase_add_test(tc_core, test_utf8_conv_utf32);
   tcase_add_test(tc_core, test_utf16_conv);
   tcase_add_test(tc_core, test_utf8_util);
   suite_add_tcase(s, tc_core);

   return s;
}

int main(void)
{
   int num_fail;
   Suite *s = create_suite();
   SRunner *sr = srunner_create(s);
   srunner_run_all(sr, CK_NORMAL);
   num_fail = srunner_ntests_failed(sr);
   srunner_free(sr);
   return (num_fail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
