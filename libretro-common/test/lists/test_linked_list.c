/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (test_linked_list.c).
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

#include <lists/linked_list.h>

#define SUITE_NAME "Linked List"

#define ARRAY_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* char arrays (not char*) so their addresses are compile-time constants and
 * can seed the static remove-case tables below. Identity comparison is what
 * the list cares about, and each symbol keeps a single stable address. */
static char _value_1[] = "value1";
static char _value_2[] = "value2";
static char _value_3[] = "value3";

/* Verify a list's length, forward iteration, and reverse iteration all agree
 * with the expected contents. Array form drives the table tests; the varargs
 * wrapper keeps the older call sites readable. */
static void _verify_list_arr(linked_list_t *list, void * const *values, int size)
{
   int i;
   linked_list_iterator_t *iterator;

   ck_assert_int_eq(linked_list_size(list), size);

   for (i = 0; i < size; i++)
      ck_assert_ptr_eq(values[i], linked_list_get(list, i));

   iterator = linked_list_iterator(list, true);
   for (i = 0; i < size; i++)
   {
      ck_assert_ptr_nonnull(iterator);
      ck_assert_ptr_eq(values[i], linked_list_iterator_value(iterator));
      iterator = linked_list_iterator_next(iterator);
   }
   ck_assert_ptr_null(iterator);

   iterator = linked_list_iterator(list, false);
   for (i = size - 1; i >= 0; i--)
   {
      ck_assert_ptr_nonnull(iterator);
      ck_assert_ptr_eq(values[i], linked_list_iterator_value(iterator));
      iterator = linked_list_iterator_next(iterator);
   }
   ck_assert_ptr_null(iterator);
}

static void _verify_list(linked_list_t *list, int size, ...)
{
   va_list values_list;
   void *values[8];
   int i;

   va_start(values_list, size);
   for (i = 0; i < size; i++)
      values[i] = va_arg(values_list, void *);
   va_end(values_list);

   _verify_list_arr(list, values, size);
}

static linked_list_t *_build_list_arr(void * const *values, int count)
{
   int i;
   linked_list_t *list = linked_list_new();

   for (i = 0; i < count; i++)
      linked_list_add(list, values[i]);

   return list;
}

START_TEST (test_linked_list_create)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_nonnull(list);
   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_free)
{
   linked_list_t *queue = linked_list_new();
   linked_list_free(queue, NULL);
   linked_list_free(NULL, NULL);
}
END_TEST

static int _free_alloced_value_count;
static void _free_alloced_value(void *value)
{
   _free_alloced_value_count++;
   free(value);
}

START_TEST (test_linked_list_free_with_fn)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, malloc(1));
   linked_list_add(list, malloc(1));
   linked_list_add(list, malloc(1));

   _free_alloced_value_count = 0;
   linked_list_free(list, &_free_alloced_value);

   ck_assert_int_eq(3, _free_alloced_value_count);
}
END_TEST

START_TEST (test_linked_list_add_null)
{
   linked_list_add(NULL, _value_1);
}
END_TEST

START_TEST (test_linked_list_insert_empty)
{
   linked_list_t *list = linked_list_new();
   linked_list_insert(list, 0, _value_1);

   ck_assert_int_eq(linked_list_size(list), 1);
   ck_assert_ptr_eq(linked_list_get(list, 0), _value_1);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_insert_first)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);
   linked_list_insert(list, 0, _value_1);

   _verify_list(list, 3, _value_1, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_insert_middle)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_3);
   linked_list_insert(list, 1, _value_2);

   _verify_list(list, 3, _value_1, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_insert_last)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_insert(list, 2, _value_3);

   _verify_list(list, 3, _value_1, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_insert_invalid)
{
   linked_list_t *list = linked_list_new();
   linked_list_insert(list, 2, _value_1);

   ck_assert_int_eq(linked_list_size(list), 0);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_insert_null)
{
   linked_list_insert(NULL, 0, _value_1);
}
END_TEST

START_TEST (test_linked_list_get_invalid)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_null(linked_list_get(list, 2));

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_get_null)
{
   ck_assert_ptr_null(linked_list_get(NULL, 0));
}
END_TEST

START_TEST (test_linked_list_get_first_matching_null)
{
   ck_assert_ptr_null(linked_list_get_first_matching(NULL, NULL, NULL));
}
END_TEST

START_TEST (test_linked_list_get_first_matching_function_null)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_null(linked_list_get_first_matching(list, NULL, NULL));

   linked_list_free(list, NULL);
}
END_TEST

bool _matches_function(void *value, void *state)
{
   ck_assert_ptr_eq(_value_1, state);
   return value == _value_2;
}

START_TEST (test_linked_list_get_first_matching_no_match)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_null(linked_list_get_first_matching(list, &_matches_function, _value_1));

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_get_last_matching_null)
{
   ck_assert_ptr_null(linked_list_get_last_matching(NULL, NULL, NULL));
}
END_TEST

START_TEST (test_linked_list_get_last_matching_function_null)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_null(linked_list_get_last_matching(list, NULL, NULL));

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_get_last_matching_no_match)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_null(linked_list_get_last_matching(list, &_matches_function, _value_1));

   linked_list_free(list, NULL);
}
END_TEST

bool _match_value_1(void *value)
{
   return _value_1 == value;
}

bool _no_match(void *value)
{
   return false;
}

/* Index-keyed removals (remove_at). */
typedef struct
{
   void  *input[4];
   int    input_len;
   size_t index;
   void  *expected_ret;
   void  *expected[4];
   int    expected_len;
} ll_remove_at_case_t;

static const ll_remove_at_case_t _remove_at_cases[] =
{
   /* empty   */ { { 0 },                            0, 0, NULL,     { 0 },                            0 },
   /* invalid */ { { _value_1, _value_2, _value_3 }, 3, 3, NULL,     { _value_1, _value_2, _value_3 }, 3 },
   /* first   */ { { _value_1, _value_2, _value_3 }, 3, 0, _value_1, { _value_2, _value_3 },           2 },
   /* middle  */ { { _value_1, _value_2, _value_3 }, 3, 1, _value_2, { _value_1, _value_3 },           2 },
   /* last    */ { { _value_1, _value_2, _value_3 }, 3, 2, _value_3, { _value_1, _value_2 },           2 },
   /* only    */ { { _value_1 },                     1, 0, _value_1, { 0 },                            0 }
};

START_TEST (test_linked_list_remove_at_param)
{
   const ll_remove_at_case_t *tc = &_remove_at_cases[_i];
   linked_list_t *list           = _build_list_arr(tc->input, tc->input_len);

   ck_assert_ptr_eq(linked_list_remove_at(list, tc->index), tc->expected_ret);
   _verify_list_arr(list, tc->expected, tc->expected_len);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_at_null)
{
   ck_assert_ptr_null(linked_list_remove_at(NULL, 0));
}
END_TEST

/* Value-keyed removals (remove_first / remove_last / remove_all). */
typedef struct
{
   void *input[4];
   int   input_len;
   void *value;
   void *expected_ret;
   void *expected[4];
   int   expected_len;
} ll_remove_value_case_t;

static const ll_remove_value_case_t _remove_first_cases[] =
{
   /* empty     */ { { 0 },                            0, _value_1, NULL,     { 0 },                            0 },
   /* not_found */ { { _value_1, _value_2, _value_3 }, 3, "foo",    NULL,     { _value_1, _value_2, _value_3 }, 3 },
   /* first     */ { { _value_1, _value_2, _value_3 }, 3, _value_1, _value_1, { _value_2, _value_3 },           2 },
   /* middle    */ { { _value_1, _value_2, _value_3 }, 3, _value_2, _value_2, { _value_1, _value_3 },           2 },
   /* last      */ { { _value_1, _value_2, _value_3 }, 3, _value_3, _value_3, { _value_1, _value_2 },           2 },
   /* only      */ { { _value_1 },                     1, _value_1, _value_1, { 0 },                            0 },
   /* multiple  */ { { _value_1, _value_2, _value_1 }, 3, _value_1, _value_1, { _value_2, _value_1 },           2 }
};

START_TEST (test_linked_list_remove_first_param)
{
   const ll_remove_value_case_t *tc = &_remove_first_cases[_i];
   linked_list_t *list              = _build_list_arr(tc->input, tc->input_len);

   ck_assert_ptr_eq(linked_list_remove_first(list, tc->value), tc->expected_ret);
   _verify_list_arr(list, tc->expected, tc->expected_len);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_first_null)
{
   ck_assert_ptr_null(linked_list_remove_first(NULL, _value_1));
}
END_TEST

static const ll_remove_value_case_t _remove_last_cases[] =
{
   /* empty     */ { { 0 },                            0, _value_1, NULL,     { 0 },                            0 },
   /* not_found */ { { _value_1, _value_2, _value_3 }, 3, "foo",    NULL,     { _value_1, _value_2, _value_3 }, 3 },
   /* first     */ { { _value_1, _value_2, _value_3 }, 3, _value_1, _value_1, { _value_2, _value_3 },           2 },
   /* middle    */ { { _value_1, _value_2, _value_3 }, 3, _value_2, _value_2, { _value_1, _value_3 },           2 },
   /* last      */ { { _value_1, _value_2, _value_3 }, 3, _value_3, _value_3, { _value_1, _value_2 },           2 },
   /* only      */ { { _value_1 },                     1, _value_1, _value_1, { 0 },                            0 },
   /* multiple  */ { { _value_1, _value_2, _value_1 }, 3, _value_1, _value_1, { _value_1, _value_2 },           2 }
};

START_TEST (test_linked_list_remove_last_param)
{
   const ll_remove_value_case_t *tc = &_remove_last_cases[_i];
   linked_list_t *list              = _build_list_arr(tc->input, tc->input_len);

   ck_assert_ptr_eq(linked_list_remove_last(list, tc->value), tc->expected_ret);
   _verify_list_arr(list, tc->expected, tc->expected_len);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_last_null)
{
   ck_assert_ptr_null(linked_list_remove_last(NULL, _value_1));
}
END_TEST

static const ll_remove_value_case_t _remove_all_cases[] =
{
   /* empty     */ { { 0 },                            0, _value_1, NULL,     { 0 },                            0 },
   /* not_found */ { { _value_1, _value_2, _value_3 }, 3, "foo",    NULL,     { _value_1, _value_2, _value_3 }, 3 },
   /* first     */ { { _value_1, _value_2, _value_3 }, 3, _value_1, _value_1, { _value_2, _value_3 },           2 },
   /* middle    */ { { _value_1, _value_2, _value_3 }, 3, _value_2, _value_2, { _value_1, _value_3 },           2 },
   /* last      */ { { _value_1, _value_2, _value_3 }, 3, _value_3, _value_3, { _value_1, _value_2 },           2 },
   /* only      */ { { _value_1 },                     1, _value_1, _value_1, { 0 },                            0 },
   /* multiple  */ { { _value_1, _value_2, _value_1 }, 3, _value_1, _value_1, { _value_2 },                     1 }
};

START_TEST (test_linked_list_remove_all_param)
{
   const ll_remove_value_case_t *tc = &_remove_all_cases[_i];
   linked_list_t *list              = _build_list_arr(tc->input, tc->input_len);

   ck_assert_ptr_eq(linked_list_remove_all(list, tc->value), tc->expected_ret);
   _verify_list_arr(list, tc->expected, tc->expected_len);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_all_null)
{
   ck_assert_ptr_null(linked_list_remove_all(NULL, _value_1));
}
END_TEST

/* Predicate-keyed removals (remove_*_matching). The first/middle/last cases
 * shuffle _value_1 to a different position rather than changing the predicate,
 * mirroring the original per-position tests. */
typedef struct
{
   void *input[4];
   int   input_len;
   bool (*match)(void *value);
   void *expected_ret;
   void *expected[4];
   int   expected_len;
} ll_remove_match_case_t;

static const ll_remove_match_case_t _remove_first_matching_cases[] =
{
   /* function_null */ { { _value_1, _value_2, _value_3 }, 3, NULL,           NULL,     { _value_1, _value_2, _value_3 }, 3 },
   /* empty         */ { { 0 },                            0, &_match_value_1, NULL,    { 0 },                            0 },
   /* not_found     */ { { _value_1, _value_2, _value_3 }, 3, &_no_match,      NULL,    { _value_1, _value_2, _value_3 }, 3 },
   /* first         */ { { _value_1, _value_2, _value_3 }, 3, &_match_value_1, _value_1,{ _value_2, _value_3 },           2 },
   /* middle        */ { { _value_2, _value_1, _value_3 }, 3, &_match_value_1, _value_1,{ _value_2, _value_3 },           2 },
   /* last          */ { { _value_2, _value_3, _value_1 }, 3, &_match_value_1, _value_1,{ _value_2, _value_3 },           2 },
   /* only          */ { { _value_1 },                     1, &_match_value_1, _value_1,{ 0 },                            0 },
   /* multiple      */ { { _value_1, _value_2, _value_1 }, 3, &_match_value_1, _value_1,{ _value_2, _value_1 },           2 }
};

START_TEST (test_linked_list_remove_first_matching_param)
{
   const ll_remove_match_case_t *tc = &_remove_first_matching_cases[_i];
   linked_list_t *list              = _build_list_arr(tc->input, tc->input_len);

   ck_assert_ptr_eq(linked_list_remove_first_matching(list, tc->match), tc->expected_ret);
   _verify_list_arr(list, tc->expected, tc->expected_len);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_first_matching_null)
{
   ck_assert_ptr_null(linked_list_remove_first_matching(NULL, &_match_value_1));
}
END_TEST

static const ll_remove_match_case_t _remove_last_matching_cases[] =
{
   /* function_null */ { { _value_1, _value_2, _value_3 }, 3, NULL,           NULL,     { _value_1, _value_2, _value_3 }, 3 },
   /* empty         */ { { 0 },                            0, &_match_value_1, NULL,    { 0 },                            0 },
   /* not_found     */ { { _value_1, _value_2, _value_3 }, 3, &_no_match,      NULL,    { _value_1, _value_2, _value_3 }, 3 },
   /* first         */ { { _value_1, _value_2, _value_3 }, 3, &_match_value_1, _value_1,{ _value_2, _value_3 },           2 },
   /* middle        */ { { _value_2, _value_1, _value_3 }, 3, &_match_value_1, _value_1,{ _value_2, _value_3 },           2 },
   /* last          */ { { _value_2, _value_3, _value_1 }, 3, &_match_value_1, _value_1,{ _value_2, _value_3 },           2 },
   /* only          */ { { _value_1 },                     1, &_match_value_1, _value_1,{ 0 },                            0 },
   /* multiple      */ { { _value_1, _value_2, _value_1 }, 3, &_match_value_1, _value_1,{ _value_1, _value_2 },           2 }
};

START_TEST (test_linked_list_remove_last_matching_param)
{
   const ll_remove_match_case_t *tc = &_remove_last_matching_cases[_i];
   linked_list_t *list              = _build_list_arr(tc->input, tc->input_len);

   ck_assert_ptr_eq(linked_list_remove_last_matching(list, tc->match), tc->expected_ret);
   _verify_list_arr(list, tc->expected, tc->expected_len);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_last_matching_null)
{
   ck_assert_ptr_null(linked_list_remove_last_matching(NULL, &_match_value_1));
}
END_TEST

/* remove_all_matching returns void, so expected_ret is unused here. */
static const ll_remove_match_case_t _remove_all_matching_cases[] =
{
   /* function_null */ { { _value_1, _value_2, _value_3 }, 3, NULL,           NULL, { _value_1, _value_2, _value_3 }, 3 },
   /* empty         */ { { 0 },                            0, &_match_value_1, NULL, { 0 },                           0 },
   /* not_found     */ { { _value_1, _value_2, _value_3 }, 3, &_no_match,      NULL, { _value_1, _value_2, _value_3 }, 3 },
   /* first         */ { { _value_1, _value_2, _value_3 }, 3, &_match_value_1, NULL, { _value_2, _value_3 },           2 },
   /* middle        */ { { _value_2, _value_1, _value_3 }, 3, &_match_value_1, NULL, { _value_2, _value_3 },           2 },
   /* last          */ { { _value_2, _value_3, _value_1 }, 3, &_match_value_1, NULL, { _value_2, _value_3 },           2 },
   /* only          */ { { _value_1 },                     1, &_match_value_1, NULL, { 0 },                            0 },
   /* multiple      */ { { _value_1, _value_2, _value_1 }, 3, &_match_value_1, NULL, { _value_2 },                     1 }
};

START_TEST (test_linked_list_remove_all_matching_param)
{
   const ll_remove_match_case_t *tc = &_remove_all_matching_cases[_i];
   linked_list_t *list              = _build_list_arr(tc->input, tc->input_len);

   linked_list_remove_all_matching(list, tc->match);
   _verify_list_arr(list, tc->expected, tc->expected_len);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_all_matching_null)
{
   linked_list_remove_all_matching(NULL, &_match_value_1);
}
END_TEST

START_TEST (test_linked_list_set_at_null)
{
   ck_assert_int_eq(linked_list_set_at(NULL, 0, _value_1) == true, 0);
}
END_TEST

START_TEST (test_linked_list_set_at_empty)
{
   linked_list_t *list = linked_list_new();
   ck_assert_int_eq(linked_list_set_at(list, 0, _value_1) == true, 0);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_set_at_invalid)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   ck_assert_int_eq(linked_list_set_at(list, 1, _value_2) == true, 0);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_iterator_remove_null)
{
   ck_assert_ptr_null(linked_list_iterator_remove(NULL));
}
END_TEST

START_TEST (test_linked_list_iterator_next_null)
{
   ck_assert_ptr_null(linked_list_iterator_next(NULL));
}
END_TEST

START_TEST (test_linked_list_iterator_value_null)
{
   ck_assert_ptr_null(linked_list_iterator_value(NULL));
}
END_TEST

START_TEST (test_linked_list_iterator_free_null)
{
   linked_list_iterator_free(NULL);
}
END_TEST

static size_t _foreach_count;
static void _foreach_fn(size_t index, void *value)
{
   _foreach_count++;
}

START_TEST (test_linked_list_foreach_null_list)
{
   linked_list_foreach(NULL, _foreach_fn);
}
END_TEST

static size_t _foreach_args_count;
static size_t _foreach_args_index[8];
static void *_foreach_args_value[8];
static void _foreach_args_fn(size_t index, void *value)
{
   if (_foreach_args_count < 8)
   {
      _foreach_args_index[_foreach_args_count] = index;
      _foreach_args_value[_foreach_args_count] = value;
   }
   _foreach_args_count++;
}

/* Checked fixture: a fresh [v1, v2, v3] list per test in the ThreeElement
 * tcase, freed on teardown. Covers the tests that all start from that exact
 * state (read-only reads, in-place set, iterator removal, foreach). */
static linked_list_t *g_list;

static void setup_three_element_list(void)
{
   g_list = linked_list_new();
   linked_list_add(g_list, _value_1);
   linked_list_add(g_list, _value_2);
   linked_list_add(g_list, _value_3);
}

static void teardown_three_element_list(void)
{
   linked_list_free(g_list, NULL);
   g_list = NULL;
}

START_TEST (test_linked_list_add)
{
   _verify_list(g_list, 3, _value_1, _value_2, _value_3);
}
END_TEST

START_TEST (test_linked_list_get_first_matching_with_match)
{
   ck_assert_ptr_eq(_value_2, linked_list_get_first_matching(g_list, &_matches_function, _value_1));
}
END_TEST

START_TEST (test_linked_list_get_last_matching_with_match)
{
   ck_assert_ptr_eq(_value_2, linked_list_get_last_matching(g_list, &_matches_function, _value_1));
}
END_TEST

static char *_replacement_value = "foo";

START_TEST (test_linked_list_set_at_first)
{
   ck_assert_int_eq(linked_list_set_at(g_list, 0, _replacement_value) == false, 0);

   _verify_list(g_list, 3, _replacement_value, _value_2, _value_3);
}
END_TEST

START_TEST (test_linked_list_set_at_middle)
{
   ck_assert_int_eq(linked_list_set_at(g_list, 1, _replacement_value) == false, 0);

   _verify_list(g_list, 3, _value_1, _replacement_value, _value_3);
}
END_TEST

START_TEST (test_linked_list_set_at_last)
{
   ck_assert_int_eq(linked_list_set_at(g_list, 2, _replacement_value) == false, 0);

   _verify_list(g_list, 3, _value_1, _value_2, _replacement_value);
}
END_TEST

START_TEST (test_linked_list_iterator_remove_first)
{
   linked_list_iterator_t *iterator;

   iterator = linked_list_iterator(g_list, true);
   iterator = linked_list_iterator_remove(iterator);

   ck_assert_ptr_nonnull(iterator);
   ck_assert_ptr_eq(linked_list_iterator_value(iterator), _value_2);
   _verify_list(g_list, 2, _value_2, _value_3);

   linked_list_iterator_free(iterator);
}
END_TEST

START_TEST (test_linked_list_iterator_remove_middle)
{
   linked_list_iterator_t *iterator;

   iterator = linked_list_iterator(g_list, true);
   iterator = linked_list_iterator_next(iterator);
   iterator = linked_list_iterator_remove(iterator);

   ck_assert_ptr_nonnull(iterator);
   ck_assert_ptr_eq(linked_list_iterator_value(iterator), _value_3);
   _verify_list(g_list, 2, _value_1, _value_3);

   linked_list_iterator_free(iterator);
}
END_TEST

START_TEST (test_linked_list_iterator_remove_last)
{
   linked_list_iterator_t *iterator;

   iterator = linked_list_iterator(g_list, true);
   iterator = linked_list_iterator_next(iterator);
   iterator = linked_list_iterator_next(iterator);
   iterator = linked_list_iterator_remove(iterator);

   ck_assert_ptr_null(iterator);
   _verify_list(g_list, 2, _value_1, _value_2);
}
END_TEST

START_TEST (test_linked_list_foreach_null_fn)
{
   linked_list_foreach(g_list, NULL);
}
END_TEST

START_TEST (test_linked_list_foreach_valid)
{
   _foreach_count = 0;
   linked_list_foreach(g_list, &_foreach_fn);
   ck_assert_uint_eq(3, _foreach_count);
}
END_TEST

START_TEST (test_linked_list_foreach_args)
{
   _foreach_args_count = 0;
   linked_list_foreach(g_list, &_foreach_args_fn);

   ck_assert_uint_eq(3, _foreach_args_count);
   ck_assert_uint_eq(0, _foreach_args_index[0]);
   ck_assert_uint_eq(1, _foreach_args_index[1]);
   ck_assert_uint_eq(2, _foreach_args_index[2]);
   ck_assert_ptr_eq(_value_1, _foreach_args_value[0]);
   ck_assert_ptr_eq(_value_2, _foreach_args_value[1]);
   ck_assert_ptr_eq(_value_3, _foreach_args_value[2]);
}
END_TEST

Suite *create_suite(void)
{
   Suite *s = suite_create(SUITE_NAME);

   TCase *tc_core = tcase_create("Core");
   tcase_add_test(tc_core, test_linked_list_create);
   tcase_add_test(tc_core, test_linked_list_free);
   tcase_add_test(tc_core, test_linked_list_free_with_fn);
   tcase_add_test(tc_core, test_linked_list_add_null);
   tcase_add_test(tc_core, test_linked_list_insert_empty);
   tcase_add_test(tc_core, test_linked_list_insert_first);
   tcase_add_test(tc_core, test_linked_list_insert_middle);
   tcase_add_test(tc_core, test_linked_list_insert_last);
   tcase_add_test(tc_core, test_linked_list_insert_invalid);
   tcase_add_test(tc_core, test_linked_list_insert_null);
   tcase_add_test(tc_core, test_linked_list_get_invalid);
   tcase_add_test(tc_core, test_linked_list_get_null);
   tcase_add_test(tc_core, test_linked_list_get_first_matching_null);
   tcase_add_test(tc_core, test_linked_list_get_first_matching_function_null);
   tcase_add_test(tc_core, test_linked_list_get_first_matching_no_match);
   tcase_add_test(tc_core, test_linked_list_get_last_matching_null);
   tcase_add_test(tc_core, test_linked_list_get_last_matching_function_null);
   tcase_add_test(tc_core, test_linked_list_get_last_matching_no_match);
   tcase_add_test(tc_core, test_linked_list_remove_at_null);
   tcase_add_test(tc_core, test_linked_list_remove_first_null);
   tcase_add_test(tc_core, test_linked_list_remove_last_null);
   tcase_add_test(tc_core, test_linked_list_remove_all_null);
   tcase_add_test(tc_core, test_linked_list_remove_first_matching_null);
   tcase_add_test(tc_core, test_linked_list_remove_last_matching_null);
   tcase_add_test(tc_core, test_linked_list_remove_all_matching_null);
   tcase_add_test(tc_core, test_linked_list_set_at_null);
   tcase_add_test(tc_core, test_linked_list_set_at_empty);
   tcase_add_test(tc_core, test_linked_list_set_at_invalid);
   tcase_add_test(tc_core, test_linked_list_iterator_remove_null);
   tcase_add_test(tc_core, test_linked_list_iterator_next_null);
   tcase_add_test(tc_core, test_linked_list_iterator_value_null);
   tcase_add_test(tc_core, test_linked_list_iterator_free_null);
   tcase_add_test(tc_core, test_linked_list_foreach_null_list);
   tcase_add_loop_test(tc_core, test_linked_list_remove_at_param,
         0, ARRAY_LEN(_remove_at_cases));
   tcase_add_loop_test(tc_core, test_linked_list_remove_first_param,
         0, ARRAY_LEN(_remove_first_cases));
   tcase_add_loop_test(tc_core, test_linked_list_remove_last_param,
         0, ARRAY_LEN(_remove_last_cases));
   tcase_add_loop_test(tc_core, test_linked_list_remove_all_param,
         0, ARRAY_LEN(_remove_all_cases));
   tcase_add_loop_test(tc_core, test_linked_list_remove_first_matching_param,
         0, ARRAY_LEN(_remove_first_matching_cases));
   tcase_add_loop_test(tc_core, test_linked_list_remove_last_matching_param,
         0, ARRAY_LEN(_remove_last_matching_cases));
   tcase_add_loop_test(tc_core, test_linked_list_remove_all_matching_param,
         0, ARRAY_LEN(_remove_all_matching_cases));
   suite_add_tcase(s, tc_core);

   {
      TCase *tc_three = tcase_create("ThreeElement");
      tcase_add_checked_fixture(tc_three, setup_three_element_list,
            teardown_three_element_list);
      tcase_add_test(tc_three, test_linked_list_add);
      tcase_add_test(tc_three, test_linked_list_get_first_matching_with_match);
      tcase_add_test(tc_three, test_linked_list_get_last_matching_with_match);
      tcase_add_test(tc_three, test_linked_list_set_at_first);
      tcase_add_test(tc_three, test_linked_list_set_at_middle);
      tcase_add_test(tc_three, test_linked_list_set_at_last);
      tcase_add_test(tc_three, test_linked_list_iterator_remove_first);
      tcase_add_test(tc_three, test_linked_list_iterator_remove_middle);
      tcase_add_test(tc_three, test_linked_list_iterator_remove_last);
      tcase_add_test(tc_three, test_linked_list_foreach_null_fn);
      tcase_add_test(tc_three, test_linked_list_foreach_valid);
      tcase_add_test(tc_three, test_linked_list_foreach_args);
      suite_add_tcase(s, tc_three);
   }

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
