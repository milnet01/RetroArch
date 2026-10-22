/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (test_config_file_write.c).
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* config_file_write() saves through a temporary file renamed over the
 * target.  The temporary must carry the target's permissions from the
 * start: otherwise a save resets a mode the user set, and a private
 * file (one holding credentials) is readable by others between the
 * rename and the caller tightening it again. */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <file/config_file.h>

#define SUITE_NAME "config_file_write"

static char target[512];

static void setup(void)
{
   snprintf(target, sizeof(target), "/tmp/cfw_test_%ld.cfg", (long)getpid());
   unlink(target);
}

static void teardown(void)
{
   char tmp_path[600];
   snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", target);
   unlink(tmp_path);
   unlink(target);
}

/* Writes a one-key config over @target, which already exists with
 * @mode, and returns the mode it is left with. */
static mode_t save_over(mode_t mode)
{
   struct stat st;
   config_file_t *conf;
   FILE *f = fopen(target, "wb");

   ck_assert(f != NULL);
   fputs("old = \"1\"\n", f);
   fclose(f);
   ck_assert_int_eq(chmod(target, mode), 0);

   conf = config_file_new_alloc();
   ck_assert(conf != NULL);
   config_set_string(conf, "secret", "value");
   ck_assert(config_file_write(conf, target, true));
   config_file_free(conf);

   ck_assert_int_eq(stat(target, &st), 0);
   return st.st_mode & 07777;
}

START_TEST (test_write_keeps_private_mode)
{
   ck_assert_int_eq(save_over(0600), 0600);
}
END_TEST

START_TEST (test_write_keeps_other_mode)
{
   /* Kept, not forced private: the writer does not decide the mode */
   ck_assert_int_eq(save_over(0640), 0640);
}
END_TEST

START_TEST (test_write_replaces_leftover_temporary)
{
   /* An interrupted save can leave the temporary behind, readable by
    * anyone who opened it; the next save makes a fresh one. */
   char tmp_path[600];
   FILE *f;

   snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", target);
   ck_assert((f = fopen(tmp_path, "wb")) != NULL);
   fclose(f);
   ck_assert_int_eq(chmod(tmp_path, 0644), 0);

   ck_assert_int_eq(save_over(0600), 0600);
   ck_assert_int_ne(access(tmp_path, F_OK), 0);
}
END_TEST

Suite *create_suite(void)
{
   Suite *s = suite_create(SUITE_NAME);
   TCase *tc_core = tcase_create("Core");

   tcase_add_checked_fixture(tc_core, setup, teardown);
   tcase_add_test(tc_core, test_write_keeps_private_mode);
   tcase_add_test(tc_core, test_write_keeps_other_mode);
   tcase_add_test(tc_core, test_write_replaces_leftover_temporary);
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
