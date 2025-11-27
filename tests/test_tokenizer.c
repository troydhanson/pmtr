/*
 * Unit Tests for pmtr Tokenizer (tok.c)
 * Tests the get_tok() function for proper token recognition
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_framework.h"
#include "../src/cfg.h"

/* External declaration of get_tok from tok.c */
int get_tok(char *c_orig, char **c, size_t *bsz, size_t *toksz, int *line);

/* Helper to tokenize a string and return token ID */
static int tokenize_single(char *input, size_t *toksz) {
    char *c = input;
    size_t bsz = strlen(input);
    int line = 1;
    return get_tok(input, &c, &bsz, toksz, &line);
}

/* Helper to get all tokens from a string */
typedef struct {
    int id;
    char token[256];
    int line;
} token_info_t;

static int tokenize_all(char *input, token_info_t *tokens, int max_tokens) {
    char *orig = input;
    char *c = input;
    size_t bsz = strlen(input);
    size_t toksz;
    int line = 1;
    int count = 0;
    int id;

    while ((id = get_tok(orig, &c, &bsz, &toksz, &line)) > 0 && count < max_tokens) {
        tokens[count].id = id;
        tokens[count].line = line;
        if (toksz < sizeof(tokens[count].token)) {
            memcpy(tokens[count].token, c, toksz);
            tokens[count].token[toksz] = '\0';
        }
        c += toksz;
        bsz -= toksz;
        count++;
    }
    return count;
}

/*
 * Basic Keyword Recognition Tests
 */
TEST_CASE(tok_keyword_job) {
    size_t toksz;
    int id = tokenize_single("job ", &toksz);
    TEST_ASSERT_EQ(TOK_JOB, id);
    TEST_ASSERT_EQ(3, toksz);
}

TEST_CASE(tok_keyword_name) {
    size_t toksz;
    int id = tokenize_single("name ", &toksz);
    TEST_ASSERT_EQ(TOK_NAME, id);
    TEST_ASSERT_EQ(4, toksz);
}

TEST_CASE(tok_keyword_cmd) {
    size_t toksz;
    int id = tokenize_single("cmd ", &toksz);
    TEST_ASSERT_EQ(TOK_CMD, id);
    TEST_ASSERT_EQ(3, toksz);
}

TEST_CASE(tok_keyword_dir) {
    size_t toksz;
    int id = tokenize_single("dir ", &toksz);
    TEST_ASSERT_EQ(TOK_DIR, id);
    TEST_ASSERT_EQ(3, toksz);
}

TEST_CASE(tok_keyword_env) {
    size_t toksz;
    int id = tokenize_single("env ", &toksz);
    TEST_ASSERT_EQ(TOK_ENV, id);
    TEST_ASSERT_EQ(3, toksz);
}

TEST_CASE(tok_keyword_out) {
    size_t toksz;
    int id = tokenize_single("out ", &toksz);
    TEST_ASSERT_EQ(TOK_OUT, id);
    TEST_ASSERT_EQ(3, toksz);
}

TEST_CASE(tok_keyword_err) {
    size_t toksz;
    int id = tokenize_single("err ", &toksz);
    TEST_ASSERT_EQ(TOK_ERR, id);
    TEST_ASSERT_EQ(3, toksz);
}

TEST_CASE(tok_keyword_in) {
    size_t toksz;
    int id = tokenize_single("in ", &toksz);
    TEST_ASSERT_EQ(TOK_IN, id);
    TEST_ASSERT_EQ(2, toksz);
}

TEST_CASE(tok_keyword_user) {
    size_t toksz;
    int id = tokenize_single("user ", &toksz);
    TEST_ASSERT_EQ(TOK_USER, id);
    TEST_ASSERT_EQ(4, toksz);
}

TEST_CASE(tok_keyword_order) {
    size_t toksz;
    int id = tokenize_single("order ", &toksz);
    TEST_ASSERT_EQ(TOK_ORDER, id);
    TEST_ASSERT_EQ(5, toksz);
}

TEST_CASE(tok_keyword_disable) {
    size_t toksz;
    int id = tokenize_single("disable ", &toksz);
    TEST_ASSERT_EQ(TOK_DISABLED, id);
    TEST_ASSERT_EQ(7, toksz);
}

TEST_CASE(tok_keyword_wait) {
    size_t toksz;
    int id = tokenize_single("wait ", &toksz);
    TEST_ASSERT_EQ(TOK_WAIT, id);
    TEST_ASSERT_EQ(4, toksz);
}

TEST_CASE(tok_keyword_once) {
    size_t toksz;
    int id = tokenize_single("once ", &toksz);
    TEST_ASSERT_EQ(TOK_ONCE, id);
    TEST_ASSERT_EQ(4, toksz);
}

TEST_CASE(tok_keyword_listen) {
    size_t toksz;
    int id = tokenize_single("listen ", &toksz);
    TEST_ASSERT_EQ(TOK_LISTEN, id);
    TEST_ASSERT_EQ(6, toksz);
}

TEST_CASE(tok_keyword_report) {
    size_t toksz;
    int id = tokenize_single("report ", &toksz);
    TEST_ASSERT_EQ(TOK_REPORT, id);
    TEST_ASSERT_EQ(6, toksz);
}

TEST_CASE(tok_keyword_bounce) {
    size_t toksz;
    int id = tokenize_single("bounce ", &toksz);
    TEST_ASSERT_EQ(TOK_BOUNCE, id);
    TEST_ASSERT_EQ(6, toksz);
}

TEST_CASE(tok_keyword_depends) {
    size_t toksz;
    int id = tokenize_single("depends ", &toksz);
    TEST_ASSERT_EQ(TOK_DEPENDS, id);
    TEST_ASSERT_EQ(7, toksz);
}

TEST_CASE(tok_keyword_ulimit) {
    size_t toksz;
    int id = tokenize_single("ulimit ", &toksz);
    TEST_ASSERT_EQ(TOK_ULIMIT, id);
    TEST_ASSERT_EQ(6, toksz);
}

TEST_CASE(tok_keyword_nice) {
    size_t toksz;
    int id = tokenize_single("nice ", &toksz);
    TEST_ASSERT_EQ(TOK_NICE, id);
    TEST_ASSERT_EQ(4, toksz);
}

TEST_CASE(tok_keyword_cpu) {
    size_t toksz;
    int id = tokenize_single("cpu ", &toksz);
    TEST_ASSERT_EQ(TOK_CPUSET, id);
    TEST_ASSERT_EQ(3, toksz);
}

/*
 * Curly Brace Tests
 */
TEST_CASE(tok_left_curly) {
    size_t toksz;
    int id = tokenize_single("{ ", &toksz);
    TEST_ASSERT_EQ(TOK_LCURLY, id);
    TEST_ASSERT_EQ(1, toksz);
}

TEST_CASE(tok_right_curly) {
    size_t toksz;
    int id = tokenize_single("} ", &toksz);
    TEST_ASSERT_EQ(TOK_RCURLY, id);
    TEST_ASSERT_EQ(1, toksz);
}

/*
 * Context-sensitive Keywords (on, to, every)
 */
TEST_CASE(tok_keyword_on) {
    size_t toksz;
    /* 'on' can appear anywhere, not just at start of line */
    int id = tokenize_single("on ", &toksz);
    TEST_ASSERT_EQ(TOK_ON, id);
    TEST_ASSERT_EQ(2, toksz);
}

TEST_CASE(tok_keyword_to) {
    size_t toksz;
    int id = tokenize_single("to ", &toksz);
    TEST_ASSERT_EQ(TOK_TO, id);
    TEST_ASSERT_EQ(2, toksz);
}

TEST_CASE(tok_keyword_every) {
    size_t toksz;
    int id = tokenize_single("every ", &toksz);
    TEST_ASSERT_EQ(TOK_EVERY, id);
    TEST_ASSERT_EQ(5, toksz);
}

/*
 * String Token Tests
 */
TEST_CASE(tok_string_simple) {
    size_t toksz;
    int id = tokenize_single("/usr/bin/test ", &toksz);
    TEST_ASSERT_EQ(TOK_STR, id);
    TEST_ASSERT_EQ(13, toksz);
}

TEST_CASE(tok_string_with_numbers) {
    size_t toksz;
    int id = tokenize_single("test123 ", &toksz);
    TEST_ASSERT_EQ(TOK_STR, id);
    TEST_ASSERT_EQ(7, toksz);
}

TEST_CASE(tok_string_path) {
    size_t toksz;
    int id = tokenize_single("/path/to/file.txt ", &toksz);
    TEST_ASSERT_EQ(TOK_STR, id);
    TEST_ASSERT_EQ(17, toksz);
}

/*
 * Quoted String Tests
 */
TEST_CASE(tok_quoted_string_simple) {
    size_t toksz;
    int id = tokenize_single("\"hello world\" ", &toksz);
    TEST_ASSERT_EQ(TOK_QUOTEDSTR, id);
    TEST_ASSERT_EQ(13, toksz);  /* includes quotes */
}

TEST_CASE(tok_quoted_string_empty) {
    size_t toksz;
    int id = tokenize_single("\"\" ", &toksz);
    TEST_ASSERT_EQ(TOK_QUOTEDSTR, id);
    TEST_ASSERT_EQ(2, toksz);
}

TEST_CASE(tok_quoted_string_with_spaces) {
    size_t toksz;
    int id = tokenize_single("\"arg with spaces\" ", &toksz);
    TEST_ASSERT_EQ(TOK_QUOTEDSTR, id);
    TEST_ASSERT_EQ(17, toksz);
}

TEST_CASE(tok_quoted_string_unterminated) {
    size_t toksz;
    int id = tokenize_single("\"unterminated", &toksz);
    TEST_ASSERT_EQ(-1, id);  /* Error: no closing quote */
}

TEST_CASE(tok_quoted_string_newline_in_middle) {
    size_t toksz;
    int id = tokenize_single("\"first\nsecond\"", &toksz);
    TEST_ASSERT_EQ(-1, id);  /* Error: newline in quoted string */
}

/*
 * Whitespace Handling Tests
 */
TEST_CASE(tok_skip_leading_spaces) {
    size_t toksz;
    int id = tokenize_single("   job ", &toksz);
    TEST_ASSERT_EQ(TOK_JOB, id);
}

TEST_CASE(tok_skip_leading_tabs) {
    size_t toksz;
    int id = tokenize_single("\t\tjob ", &toksz);
    TEST_ASSERT_EQ(TOK_JOB, id);
}

TEST_CASE(tok_skip_mixed_whitespace) {
    size_t toksz;
    int id = tokenize_single("  \t  \t job ", &toksz);
    TEST_ASSERT_EQ(TOK_JOB, id);
}

/*
 * Line Number Tracking Tests
 */
TEST_CASE(tok_line_number_increment) {
    char input[] = "\njob ";
    char *c = input;
    size_t bsz = strlen(input);
    size_t toksz;
    int line = 1;

    int id = get_tok(input, &c, &bsz, &toksz, &line);
    TEST_ASSERT_EQ(TOK_JOB, id);
    TEST_ASSERT_EQ(2, line);  /* Line incremented due to newline */
}

TEST_CASE(tok_line_number_multiple_newlines) {
    char input[] = "\n\n\njob ";
    char *c = input;
    size_t bsz = strlen(input);
    size_t toksz;
    int line = 1;

    int id = get_tok(input, &c, &bsz, &toksz, &line);
    TEST_ASSERT_EQ(TOK_JOB, id);
    TEST_ASSERT_EQ(4, line);  /* 3 newlines = line 4 */
}

/*
 * Comment Handling Tests
 */
TEST_CASE(tok_comment_skip) {
    char input[] = "# this is a comment\njob ";
    char *c = input;
    size_t bsz = strlen(input);
    size_t toksz;
    int line = 1;

    int id = get_tok(input, &c, &bsz, &toksz, &line);
    TEST_ASSERT_EQ(TOK_JOB, id);
    TEST_ASSERT_EQ(2, line);
}

TEST_CASE(tok_comment_at_end) {
    char input[] = "# only a comment";
    char *c = input;
    size_t bsz = strlen(input);
    size_t toksz;
    int line = 1;

    int id = get_tok(input, &c, &bsz, &toksz, &line);
    TEST_ASSERT_EQ(0, id);  /* End of input */
}

TEST_CASE(tok_multiple_comments) {
    char input[] = "# comment 1\n# comment 2\njob ";
    char *c = input;
    size_t bsz = strlen(input);
    size_t toksz;
    int line = 1;

    int id = get_tok(input, &c, &bsz, &toksz, &line);
    TEST_ASSERT_EQ(TOK_JOB, id);
    TEST_ASSERT_EQ(3, line);
}

/*
 * End of Input Tests
 */
TEST_CASE(tok_empty_input) {
    size_t toksz;
    int id = tokenize_single("", &toksz);
    TEST_ASSERT_EQ(0, id);
}

TEST_CASE(tok_whitespace_only) {
    size_t toksz;
    int id = tokenize_single("   \t  \n  ", &toksz);
    TEST_ASSERT_EQ(0, id);
}

/*
 * Keyword at Non-Line-Start Tests (should become TOK_STR)
 */
TEST_CASE(tok_keyword_after_text_becomes_string) {
    /* When 'name' appears after other text on line, it's treated as string */
    token_info_t tokens[10];
    char input[] = "cmd /usr/bin/name\n";
    int count = tokenize_all(input, tokens, 10);

    TEST_ASSERT_EQ(2, count);
    TEST_ASSERT_EQ(TOK_CMD, tokens[0].id);
    /* The path contains 'name' but it's not at line start, so it's a string */
    TEST_ASSERT_EQ(TOK_STR, tokens[1].id);
}

/*
 * Multi-Token Sequence Tests
 */
TEST_CASE(tok_job_block_sequence) {
    token_info_t tokens[20];
    char input[] = "job {\n  name test\n  cmd /bin/echo\n}\n";
    int count = tokenize_all(input, tokens, 20);

    TEST_ASSERT_EQ(7, count);
    TEST_ASSERT_EQ(TOK_JOB, tokens[0].id);
    TEST_ASSERT_EQ(TOK_LCURLY, tokens[1].id);
    TEST_ASSERT_EQ(TOK_NAME, tokens[2].id);
    TEST_ASSERT_EQ(TOK_STR, tokens[3].id);  /* "test" */
    TEST_ASSERT_EQ(TOK_CMD, tokens[4].id);
    TEST_ASSERT_EQ(TOK_STR, tokens[5].id);  /* "/bin/echo" */
    TEST_ASSERT_EQ(TOK_RCURLY, tokens[6].id);
}

TEST_CASE(tok_listen_sequence) {
    token_info_t tokens[10];
    char input[] = "listen on udp://127.0.0.1:9999\n";
    int count = tokenize_all(input, tokens, 10);

    TEST_ASSERT_EQ(3, count);
    TEST_ASSERT_EQ(TOK_LISTEN, tokens[0].id);
    TEST_ASSERT_EQ(TOK_ON, tokens[1].id);
    TEST_ASSERT_EQ(TOK_STR, tokens[2].id);
}

TEST_CASE(tok_report_sequence) {
    token_info_t tokens[10];
    char input[] = "report to udp://192.168.1.1:8080\n";
    int count = tokenize_all(input, tokens, 10);

    TEST_ASSERT_EQ(3, count);
    TEST_ASSERT_EQ(TOK_REPORT, tokens[0].id);
    TEST_ASSERT_EQ(TOK_TO, tokens[1].id);
    TEST_ASSERT_EQ(TOK_STR, tokens[2].id);
}

TEST_CASE(tok_bounce_sequence) {
    token_info_t tokens[10];
    char input[] = "bounce every 1d\n";
    int count = tokenize_all(input, tokens, 10);

    TEST_ASSERT_EQ(3, count);
    TEST_ASSERT_EQ(TOK_BOUNCE, tokens[0].id);
    TEST_ASSERT_EQ(TOK_EVERY, tokens[1].id);
    TEST_ASSERT_EQ(TOK_STR, tokens[2].id);
}

TEST_CASE(tok_ulimit_sequence) {
    token_info_t tokens[10];
    char input[] = "ulimit -n 1024\n";
    int count = tokenize_all(input, tokens, 10);

    TEST_ASSERT_EQ(3, count);
    TEST_ASSERT_EQ(TOK_ULIMIT, tokens[0].id);
    TEST_ASSERT_EQ(TOK_STR, tokens[1].id);  /* "-n" */
    TEST_ASSERT_EQ(TOK_STR, tokens[2].id);  /* "1024" */
}

TEST_CASE(tok_depends_block_sequence) {
    token_info_t tokens[10];
    char input[] = "depends {\n  /path/to/file\n  /another/file\n}\n";
    int count = tokenize_all(input, tokens, 10);

    TEST_ASSERT_EQ(5, count);
    TEST_ASSERT_EQ(TOK_DEPENDS, tokens[0].id);
    TEST_ASSERT_EQ(TOK_LCURLY, tokens[1].id);
    TEST_ASSERT_EQ(TOK_STR, tokens[2].id);
    TEST_ASSERT_EQ(TOK_STR, tokens[3].id);
    TEST_ASSERT_EQ(TOK_RCURLY, tokens[4].id);
}

TEST_CASE(tok_cmd_with_quoted_args) {
    token_info_t tokens[10];
    char input[] = "cmd /bin/echo \"hello world\" arg2\n";
    int count = tokenize_all(input, tokens, 10);

    TEST_ASSERT_EQ(4, count);
    TEST_ASSERT_EQ(TOK_CMD, tokens[0].id);
    TEST_ASSERT_EQ(TOK_STR, tokens[1].id);        /* "/bin/echo" */
    TEST_ASSERT_EQ(TOK_QUOTEDSTR, tokens[2].id);  /* "hello world" */
    TEST_ASSERT_EQ(TOK_STR, tokens[3].id);        /* "arg2" */
}

/*
 * Edge Cases
 */
TEST_CASE(tok_keyword_at_eof_without_space) {
    /* Keywords at end of input (buffer) ARE recognized as keywords.
     * The tokenizer treats end-of-buffer as equivalent to whitespace. */
    char input[] = "job";
    char *c = input;
    size_t bsz = strlen(input);
    size_t toksz;
    int line = 1;

    /* At EOF, keyword IS recognized (special case in tokenizer) */
    int id = get_tok(input, &c, &bsz, &toksz, &line);
    TEST_ASSERT_EQ(TOK_JOB, id);
}

TEST_CASE(tok_keyword_partial_match) {
    /* "jobs" should be TOK_STR, not TOK_JOB - the 's' after 'job' prevents match */
    size_t toksz;
    int id = tokenize_single("jobs ", &toksz);
    TEST_ASSERT_EQ(TOK_STR, id);
    TEST_ASSERT_EQ(4, toksz);
}

TEST_CASE(tok_curly_without_space) {
    /* Curly braces require trailing whitespace just like other tokens.
     * "{name" will be parsed as one string because '{' isn't followed by space. */
    token_info_t tokens[10];
    char input[] = "{name";
    int count = tokenize_all(input, tokens, 10);

    /* The entire "{name" is treated as a single string token */
    TEST_ASSERT_EQ(1, count);
    TEST_ASSERT_EQ(TOK_STR, tokens[0].id);
}

TEST_CASE(tok_multiple_curlies) {
    /* "{{}}" without whitespace between braces is treated as one string */
    token_info_t tokens[10];
    char input[] = "{{}}";
    int count = tokenize_all(input, tokens, 10);

    /* All four braces together form one string token */
    TEST_ASSERT_EQ(1, count);
    TEST_ASSERT_EQ(TOK_STR, tokens[0].id);
}

TEST_CASE(tok_curlies_with_newlines) {
    /* Most keywords (including RCURLY) must be at start of line.
     * Only LCURLY, ON, TO, EVERY can appear after other tokens on a line.
     * This test verifies that braces on separate lines are recognized. */
    token_info_t tokens[10];
    char input[] = "{\n{\n}\n}\n";
    int count = tokenize_all(input, tokens, 10);

    TEST_ASSERT_EQ(4, count);
    TEST_ASSERT_EQ(TOK_LCURLY, tokens[0].id);
    TEST_ASSERT_EQ(TOK_LCURLY, tokens[1].id);
    TEST_ASSERT_EQ(TOK_RCURLY, tokens[2].id);
    TEST_ASSERT_EQ(TOK_RCURLY, tokens[3].id);
}

/*
 * Test Runner
 */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    TEST_SUITE_BEGIN("Tokenizer Basic Keywords");
    RUN_TEST(tok_keyword_job);
    RUN_TEST(tok_keyword_name);
    RUN_TEST(tok_keyword_cmd);
    RUN_TEST(tok_keyword_dir);
    RUN_TEST(tok_keyword_env);
    RUN_TEST(tok_keyword_out);
    RUN_TEST(tok_keyword_err);
    RUN_TEST(tok_keyword_in);
    RUN_TEST(tok_keyword_user);
    RUN_TEST(tok_keyword_order);
    RUN_TEST(tok_keyword_disable);
    RUN_TEST(tok_keyword_wait);
    RUN_TEST(tok_keyword_once);
    RUN_TEST(tok_keyword_listen);
    RUN_TEST(tok_keyword_report);
    RUN_TEST(tok_keyword_bounce);
    RUN_TEST(tok_keyword_depends);
    RUN_TEST(tok_keyword_ulimit);
    RUN_TEST(tok_keyword_nice);
    RUN_TEST(tok_keyword_cpu);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Tokenizer Braces");
    RUN_TEST(tok_left_curly);
    RUN_TEST(tok_right_curly);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Tokenizer Context Keywords");
    RUN_TEST(tok_keyword_on);
    RUN_TEST(tok_keyword_to);
    RUN_TEST(tok_keyword_every);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Tokenizer Strings");
    RUN_TEST(tok_string_simple);
    RUN_TEST(tok_string_with_numbers);
    RUN_TEST(tok_string_path);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Tokenizer Quoted Strings");
    RUN_TEST(tok_quoted_string_simple);
    RUN_TEST(tok_quoted_string_empty);
    RUN_TEST(tok_quoted_string_with_spaces);
    RUN_TEST(tok_quoted_string_unterminated);
    RUN_TEST(tok_quoted_string_newline_in_middle);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Tokenizer Whitespace");
    RUN_TEST(tok_skip_leading_spaces);
    RUN_TEST(tok_skip_leading_tabs);
    RUN_TEST(tok_skip_mixed_whitespace);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Tokenizer Line Numbers");
    RUN_TEST(tok_line_number_increment);
    RUN_TEST(tok_line_number_multiple_newlines);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Tokenizer Comments");
    RUN_TEST(tok_comment_skip);
    RUN_TEST(tok_comment_at_end);
    RUN_TEST(tok_multiple_comments);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Tokenizer End of Input");
    RUN_TEST(tok_empty_input);
    RUN_TEST(tok_whitespace_only);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Tokenizer Context Sensitivity");
    RUN_TEST(tok_keyword_after_text_becomes_string);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Tokenizer Multi-Token Sequences");
    RUN_TEST(tok_job_block_sequence);
    RUN_TEST(tok_listen_sequence);
    RUN_TEST(tok_report_sequence);
    RUN_TEST(tok_bounce_sequence);
    RUN_TEST(tok_ulimit_sequence);
    RUN_TEST(tok_depends_block_sequence);
    RUN_TEST(tok_cmd_with_quoted_args);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Tokenizer Edge Cases");
    RUN_TEST(tok_keyword_at_eof_without_space);
    RUN_TEST(tok_keyword_partial_match);
    RUN_TEST(tok_curly_without_space);
    RUN_TEST(tok_multiple_curlies);
    RUN_TEST(tok_curlies_with_newlines);
    TEST_SUITE_END();

    print_test_results();
    return get_test_exit_code();
}
