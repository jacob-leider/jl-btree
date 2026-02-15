#include "./btree_tests.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include "./btree.h"
#include "./btree_node.h"
#include "./btree_print.h"
#include "./printutils.h"
#include "./serialize.h"
#include "./testutils.h"

static int TestDidntExecute(int test_num)
{
    printf(RED "========== Test %d Didn't Execute ==========\n", test_num);
    return 0;
}

void PrintPass(int test_num, const char* test_name)
{
    char test_name_in_quotes[100];
    sprintf(test_name_in_quotes, "\"%s\"", test_name);
    printf("\t- Test %-20s [%d]: " GREEN "Passed\n" RESET, test_name_in_quotes,
        test_num);
}

void PrintFail(int test_num, int res, int exp)
{
    printf("Test %d: " RED "Failed\n\tRes: %d\n\tExp: %d\n" RESET, test_num,
        res, exp);
}

static void PrintFailureReason(
    int test_num, const char* reason, const char* test_name)
{
    char test_name_in_quotes[100];
    sprintf(test_name_in_quotes, "\"%s\"", test_name);
    printf("\t- Test %-20s [%d]: " RED
           "Failed: "
           "%s not as expected\n" RESET,
        test_name_in_quotes, test_num, reason);
}

static void PrintCompTrees(BTreeNode* exp, BTreeNode* res)
{
    printf("\nExpected: ");
    btree_subtree_in_order_traverse(exp);
    printf("Recieved: ");
    btree_subtree_in_order_traverse(res);
    printf("\n");
}

void PrintBeginTest(const char* func)
{
    // Let's convert the PascalCase test names to snake_case. There was
    // literally zero reason to have different cases, but this was really fun to
    // write. I mean look at that thing...
    char s[100];  // If they get longer than this you need to do something more
                  // productive with your time. Please
    s[0]    = 'b';
    s[1]    = 't';
    char *i = (char*)func + 6, *j = s + 2;
    while (*i)
    {
        if (*i < 'a')
        {
            *j++ = '_';
            *j++ = *i++ + ('a' - 'A');
        }
        else
        {
            *j++ = *i++;
        }
    }

    *j = '\0';

    // Terminal size
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);

    for (int i = 0; i < w.ws_col; i++) printf("-");
    printf(COLOR_BOLD "%s\n\n" COLOR_OFF, s);
}

typedef struct InsertTestCase
{
    const char* before;
    const char* after;
    int key;
    int exp_rc;
} InsertTestCase;

int BuildInsertTestCase(InsertTestCase* test_case, int test_num)
{
    char path[100];
    sprintf(path, "./test/cases/%s/case%d.json", "insert", test_num);
    FILE* fctr = fopen(path, "r");
    FILE* fptr = fopen(path, "r");

    if (fctr == NULL) return 0;

    int len = 0;
    char c;
    while ((c = fgetc(fctr)) != EOF)
    {
        len += 1;
    }

    char* test_case_str = (char*)malloc((len + 1) * sizeof(char));

    int i               = 0;
    while ((c = fgetc(fptr)) != EOF)
    {
        test_case_str[i] = c;
        i++;
    }

    int in_str       = 0;
    int depth        = 0;
    int str_start    = 0;
    int kw_start     = 0;
    int in_kw        = 0;
    int str_size     = 0;
    int kw_size      = 0;
    int in_quote     = 0;
    int before_colon = 1;

    for (int i = 0; i < len; i++)
    {
        c = test_case_str[i];

        if (c == '{')
        {
            depth += 1;
        }
        else if (c == '}')
        {
            depth -= 1;
        }
        else if (c == '"')
        {
            if (in_quote)
            {
                if (before_colon)
                {
                    // in keyword
                }
                else
                {
                    // in value
                }
            }
            else
            {
                in_quote = 1;
            }

            if (in_kw)
            {
                // TODO: copy word from str_start to str_start + str_size

                if (kw_size == strlen("before") &&
                    strncmp(test_case_str + kw_start, "before", kw_size))
                {
                    printf("before found\n");
                }
                else if (kw_size == strlen("after") &&
                         strncmp(test_case_str + kw_start, "after", kw_size))
                {
                    printf("after found\n");
                }
                else if (kw_size == strlen("node size") &&
                         strncmp(
                             test_case_str + kw_start, "node size", kw_size))
                {
                    printf("node size found\n");
                }
                else if (kw_size == strlen("exp rc") &&
                         strncmp(test_case_str + kw_start, "exp rc", kw_size))
                {
                    printf("exp rc found\n");
                }
                else
                {
                    printf("%s found\n", test_case_str + kw_start);
                }

                in_kw = 0;
            }
            else if (in_str)
            {
                // Get string
                in_str = 0;
            }
            else
            {
                if (in_kw)
                {
                    kw_start = i + 1;
                    kw_size  = 0;
                }
                else
                {
                    str_start = i + 1;
                    str_size  = 0;
                }

                in_str = 1;
            }
        }
        else if (c == ':')
        {
            before_colon = 0;
        }
        else if (c == ',')
        {
            before_colon = 1;
        }

        kw_size += 1;
        str_size += 1;
    }

    // Close the file
    fclose(fctr);
    fclose(fptr);
    free(test_case_str);
    return 1;
}

void PrintTestStats(int passed, int total)
{
    printf(RESET "\nResults:%s%s %d / %d Tests Passed\n%s%s", COLOR_BOLD, GREEN,
        passed, total, RESET, COLOR_OFF);
}

int TestBTreeNodeInsertImplCase(int* test_num,
    int* num_passed,
    const char* test_name,
    int node_size,
    const char* before_str,
    int val,
    const char* exp_after_str,
    int exp_rc)
{
    // InsertTestCase test_case;
    // BuildInsertTestCase(&test_case, 1);

    DeserializationSettings settings = {
        .node_size = node_size, .lexer_settings = NULL};

    *test_num += 1;
    BTreeNode *before = NULL, *exp_after = NULL;
    if (!TreeFromStr(before_str, strlen(before_str), &settings, &before) ||
        !TreeFromStr(
            exp_after_str, strlen(exp_after_str), &settings, &exp_after))
        return 0;

    BTreeNode* after = NULL;
    int rc           = btree_node_insert_impl(before, val, &after);
    if (after == NULL) after = before;

    if (rc != exp_rc)
    {
        PrintFailureReason(*test_num, "return code", test_name);
        printf("exp rc: %d, rc: %d\n", exp_rc, rc);
        return 1;
    }

    if (!btree_cmp(after, exp_after))
    {
        PrintFailureReason(*test_num, "tree", test_name);
        PrintCompTrees(exp_after, after);
        return 1;
    }

    // check sizes
    if (!btree_check_subtree_sizes(after))
    {
        PrintFailureReason(*test_num, "a subtree size", test_name);
        return 1;
    }

    // Ironically, this may not even work if the test failed...
    btree_subtree_kill(after);
    btree_subtree_kill(exp_after);

    PrintPass(*test_num, test_name);
    *num_passed += 1;
    return 1;
}

int TestBTreeNodeInsertImpl()
{
    int test_num = 0, num_passed = 0;

    // 1. `key` is in the tree.
    // 2. `leaf` is full
    // 3. `leaf`'s parent is full
    // 4. Every ancestor of `leaf` is full

    PrintBeginTest(__func__);

    // Case 1
    //
    // 1 2 3 4
    //
    {
        const char* test_name  = "key in tree";

        const int size         = 3;

        const char* before_str = "10 30";

        const char* after_str  = "10 30";

        int val = 10, exp_rc = 2;

        if (!TestBTreeNodeInsertImplCase(&test_num, &num_passed, test_name,
                size, before_str, val, after_str, exp_rc))
        {
            return TestDidntExecute(test_num);
        }
    }
    // case 2 -> 8: Trivial, similar to 1
    {
    }
    // Case 9: Leaf is not full (easy)
    // 1 2 3 4
    // ^
    {
        const char* test_name = "leaf not full";

        const int size = 3, val = 2, exp_rc = 1;

        const char* before_str =
            "((1 3) 10 (11 12 13) 20 (21 22 23) 30 (31 32 33)) 100 ((101 102 "
            "103) 110 (111 112 113) 120 (121 122 123) 130 (131 132 133)) 200 "
            "((201 202 203) 210 (211 212 213) 220 (221 222 223) 230 (231 232 "
            "233)) 300 ((301 302 303) 310 (311 312 313) 320 (321 322 323) 330 "
            "(331 332 333))";

        const char* after_str =
            "((1 2 3) 10 (11 12 13) 20 (21 22 23) 30 (31 32 33)) 100 ((101 102 "
            "103) 110 (111 112 113) 120 (121 122 123) 130 (131 132 133)) 200 "
            "((201 202 203) 210 (211 212 213) 220 (221 222 223) 230 (231 232 "
            "233)) 300 ((301 302 303) 310 (311 312 313) 320 (321 322 323) 330 "
            "(331 332 333))";

        if (!TestBTreeNodeInsertImplCase(&test_num, &num_passed, test_name,
                size, before_str, val, after_str, exp_rc))
        {
            return TestDidntExecute(test_num);
        }
    }
    // Case 10-12: Similar to 9 (may still be worth testing)
    {
    }
    // Case 13.1: Leaf is full, leaf's parent is not full, key is on the left
    // side of leaf 1 2 3 4 ^ ^
    {
        const char* test_name = "leaf full, left";

        const int size = 3, val = 109, exp_rc = 1;

        const char* before_str =
            "((10 20 30) 100 (110 120 130) 200 (210 220 230)) 1000 ((1010 1020 "
            "1030) 1100 (1110 1120 1130) 1200 (1210 1220 1230) 1300 (1310 1320 "
            "1330)) 2000 ((2010 2020 2030) 2100 (2110 2120 2130) 2200 (2210 "
            "2220 2230) 2300 (2310 2320 2330)) 3000 ((3010 3020 3030) 3100 "
            "(3110 3120 3130) 3200 (3210 3220 3230) 3300 (3310 3320 3330))";

        const char* after_str =
            "((10 20 30) 100 (109 110) 120 (130) 200 (210 220 230)) 1000 "
            "((1010 1020 1030) 1100 (1110 1120 1130) 1200 (1210 1220 1230) "
            "1300 (1310 1320 1330)) 2000 ((2010 2020 2030) 2100 (2110 2120 "
            "2130) 2200 (2210 2220 2230) 2300 (2310 2320 2330)) 3000 ((3010 "
            "3020 3030) 3100 (3110 3120 3130) 3200 (3210 3220 3230) 3300 (3310 "
            "3320 3330))";

        if (!TestBTreeNodeInsertImplCase(&test_num, &num_passed, test_name,
                size, before_str, val, after_str, exp_rc))
        {
            return TestDidntExecute(test_num);
        }
    }
    // Case 13.2: Leaf is full, leaf's parent is not full, key is on the right
    // side of leaf 1 2 3 4 ^ ^
    {
        const char* test_name = "leaf full, right";

        const int size = 3, val = 131, exp_rc = 1;

        const char* before_str =
            "((10 20 30) 100 (110 120 130) 200 (210 220 230)) 1000 ((1010 1020 "
            "1030) 1100 (1110 1120 1130) 1200 (1210 1220 1230) 1300 (1310 1320 "
            "1330)) 2000 ((2010 2020 2030) 2100 (2110 2120 2130) 2200 (2210 "
            "2220 2230) 2300 (2310 2320 2330)) 3000 ((3010 3020 3030) 3100 "
            "(3110 3120 3130) 3200 (3210 3220 3230) 3300 (3310 3320 3330))";

        const char* after_str =
            "((10 20 30) 100 (110) 120 (130 131) 200 (210 220 230)) 1000 "
            "((1010 1020 1030) 1100 (1110 1120 1130) 1200 (1210 1220 1230) "
            "1300 (1310 1320 1330)) 2000 ((2010 2020 2030) 2100 (2110 2120 "
            "2130) 2200 (2210 2220 2230) 2300 (2310 2320 2330)) 3000 ((3010 "
            "3020 3030) 3100 (3110 3120 3130) 3200 (3210 3220 3230) 3300 (3310 "
            "3320 3330))";

        if (!TestBTreeNodeInsertImplCase(&test_num, &num_passed, test_name,
                size, before_str, val, after_str, exp_rc))
        {
            return TestDidntExecute(test_num);
        }
    }
    // Case 13.3: Leaf is full, leaf's parent is not full, key is in the middle
    // 1 2 3 4
    // ^ ^
    {
        const char* test_name = "leaf full, middle";

        const int size = 3, val = 128, exp_rc = 1;

        const char* before_str =
            "((10 20 30) 100 (110 120 130) 200 (210 220 230)) 1000 ((1010 1020 "
            "1030) 1100 (1110 1120 1130) 1200 (1210 1220 1230) 1300 (1310 1320 "
            "1330)) 2000 ((2010 2020 2030) 2100 (2110 2120 2130) 2200 (2210 "
            "2220 2230) 2300 (2310 2320 2330)) 3000 ((3010 3020 3030) 3100 "
            "(3110 3120 3130) 3200 (3210 3220 3230) 3300 (3310 3320 3330))";

        const char* after_str =
            "((10 20 30) 100 (110) 120 (128 130) 200 (210 220 230)) 1000 "
            "((1010 1020 1030) 1100 (1110 1120 1130) 1200 (1210 1220 1230) "
            "1300 (1310 1320 1330)) 2000 ((2010 2020 2030) 2100 (2110 2120 "
            "2130) 2200 (2210 2220 2230) 2300 (2310 2320 2330)) 3000 ((3010 "
            "3020 3030) 3100 (3110 3120 3130) 3200 (3210 3220 3230) 3300 (3310 "
            "3320 3330))";

        if (!TestBTreeNodeInsertImplCase(&test_num, &num_passed, test_name,
                size, before_str, val, after_str, exp_rc))
        {
            return TestDidntExecute(test_num);
        }
    }
    // Case 14.1: Leaf is full, every ancestor of leaf is full (new root)
    // 1 2 3 4
    // ^ ^   ^
    {
        const char* test_name = "complete tree 2";

        const int size = 3, val = 1121, exp_rc = 1;

        const char* before_str =
            "((10 20 30) 100 (110 120 130) 200 (210 220 230) 300 (310 320 "
            "330)) 1000 ((1010 1020 1030) 1100 (1110 1120 1130) 1200 (1210 "
            "1220 1230) 1300 (1310 1320 1330)) 2000 ((2010 2020 2030) 2100 "
            "(2110 2120 2130) 2200 (2210 2220 2230) 2300 (2310 2320 2330)) "
            "3000 ((3010 3020 3030) 3100 (3110 3120 3130) 3200 (3210 3220 "
            "3230) 3300 (3310 3320 3330))";

        const char* after_str =
            "(((10 20 30) 100 (110 120 130) 200 (210 220 230) 300 (310 320 "
            "330)) 1000 ((1010 1020 1030) 1100 (1110) 1120 (1121 1130)) 1200 "
            "((1210 1220 1230) 1300 (1310 1320 1330))) 2000 (((2010 2020 2030) "
            "2100 (2110 2120 2130) 2200 (2210 2220 2230) 2300 (2310 2320 "
            "2330)) 3000 ((3010 3020 3030) 3100 (3110 3120 3130) 3200 (3210 "
            "3220 3230) 3300 (3310 3320 3330)))";

        if (!TestBTreeNodeInsertImplCase(&test_num, &num_passed, test_name,
                size, before_str, val, after_str, exp_rc))
        {
            return TestDidntExecute(test_num);
        }
    }
    // Case 14.2
    // 1 2 3 4
    // ^ ^   ^
    {
        const char* test_name = "";

        const int size = 3, val = 3333, exp_rc = 1;

        const char* before_str =
            "((10 20 30) 100 (110 120 130) 200 (210 220 230) 300 (310 320 "
            "330)) 1000 ((1010 1020 1030) 1100 (1110 1120 1130) 1200 (1210 "
            "1220 1230) 1300 (1310 1320 1330)) 2000 ((2010 2020 2030) 2100 "
            "(2110 2120 2130) 2200 (2210 2220 2230) 2300 (2310 2320 2330)) "
            "3000 ((3010 3020 3030) 3100 (3110 3120 3130) 3200 (3210 3220 "
            "3230) 3300 (3310 3320 3330))";

        const char* after_str =
            "(((10 20 30) 100 (110 120 130) 200 (210 220 230) 300 (310 320 "
            "330)) 1000 ((1010 1020 1030) 1100 (1110 1120 1130) 1200 (1210 "
            "1220 1230) 1300 (1310 1320 1330))) 2000 (((2010 2020 2030) 2100 "
            "(2110 2120 2130) 2200 (2210 2220 2230) 2300 (2310 2320 2330)) "
            "3000 ((3010 3020 3030) 3100 (3110 3120 3130)) 3200 ((3210 3220 "
            "3230) 3300 (3310) 3320 (3330 3333)))";

        if (!TestBTreeNodeInsertImplCase(&test_num, &num_passed, test_name,
                size, before_str, val, after_str, exp_rc))
        {
            return TestDidntExecute(test_num);
        }
    }
    // Case 15: Leaf is full, parent is full, there exist an ancestor of leaf
    // that is not full (perhaps redundant) 1 2 3 4 ^ ^ ^
    {
        const char* test_name = "";

        const int size = 3, val = 111, exp_rc = 1;

        const char* before_str =
            "((10 20 30) 100 (110 120 130) 200 (210 220 230) 300 (310 320 "
            "330)) 1000 ((1010 1020 1030) 1100 (1110 1120 1130) 1200 (1210 "
            "1220 1230) 1300 (1310 1320 1330)) 2000 ((2010 2020 2030) 2100 "
            "(2110 2120 2130) 2200 (2210 2220 2230) 2300 (2310 2320 2330))";

        const char* after_str =
            "((10 20 30) 100 (110 111) 120 (130)) 200 ((210 220 230) 300 (310 "
            "320 330)) 1000 ((1010 1020 1030) 1100 (1110 1120 1130) 1200 (1210 "
            "1220 1230) 1300 (1310 1320 1330)) 2000 ((2010 2020 2030) 2100 "
            "(2110 2120 2130) 2200 (2210 2220 2230) 2300 (2310 2320 2330))";

        if (!TestBTreeNodeInsertImplCase(&test_num, &num_passed, test_name,
                size, before_str, val, after_str, exp_rc))
        {
            return TestDidntExecute(test_num);
        }
    }
    // Case 16: Redundant
    {
    }

    PrintTestStats(num_passed, test_num);
    return 1;
}

int TestBTreeNodeDeleteImplCase(int* test_num,
    int* num_passed,
    const char* test_name,
    int node_size,
    const char* before_tree_str,
    int val,
    const char* after_tree_str,
    int exp_rc)
{
    *test_num += 1;

    DeserializationSettings settings = {
        .node_size = node_size, .lexer_settings = NULL};

    BTreeNode* before_root;
    if (!TreeFromStr(
            before_tree_str, strlen(before_tree_str), &settings, &before_root))
        return 0;

    // BTreeNode* exp_after_root;
    // if (!TreeFromStr(
    //         after_tree_str, strlen(after_tree_str), node_size,
    //         &exp_after_root))
    //     return 0;

    BTreeNode* after_root = NULL;
    int rc = btree_node_delete_impl(before_root, val, &after_root);
    if (!after_root) after_root = before_root;

    if (rc != exp_rc)
    {
        PrintFailureReason(*test_num, "return code", test_name);
        return 1;
    }

    if (strncmp(StrFromTree(after_root), after_tree_str,
            strlen(after_tree_str)) != 0)
    {
        PrintFailureReason(*test_num, "tree", test_name);
        printf("\nexpected: \"%s\"", after_tree_str);
        printf("recieved: \"%s\"\n\n", StrFromTree(after_root));
        return 1;
    }

    // check sizes
    if (!btree_check_subtree_sizes(after_root))
    {
        PrintFailureReason(*test_num, "a subtree size", test_name);
        return 1;
    }

    btree_subtree_kill(after_root);
    // btree_subtree_kill(exp_after_root);

    PrintPass(*test_num, test_name);

    *num_passed += 1;
    return 1;
}

int TestBTreeNodeDeleteImpl()
{
    int test_num = 0, num_passed = 0;
    PrintBeginTest(__func__);

    // Case 1
    {
        const char* test_name = "";
        int size              = 4;
        const char* before    = "1 2 3 4";
        int val               = 3;
        const char* after     = "(1 2 4)";
        int exp_rc            = 1;

        if (!TestBTreeNodeDeleteImplCase(&test_num, &num_passed, test_name,
                size, before, val, after, exp_rc))
            return TestDidntExecute(test_num);
    }
    // Case 2
    {
        const char* test_name = "";
        int size              = 4;
        const char* before    = "(1 2 3 4) 10 (11 12 13 14)";
        int val               = 3;
        const char* after     = "((1 2 4) 10 (11 12 13 14))";
        int exp_rc            = 1;

        if (!TestBTreeNodeDeleteImplCase(&test_num, &num_passed, test_name,
                size, before, val, after, exp_rc))
            return TestDidntExecute(test_num);
    }
    // Case 3: Leaf borrows a key from a sibling
    //
    // Expected execution:
    //      - descend once
    //      - rotate left about 20
    {
        const char* test_name = "";
        int size              = 4;
        const char* before    = "(1 2 3 4) 10 (11) 20 (21 22 23)";
        int val               = 11;
        const char* after     = "((1 2 3) 4 (10) 20 (21 22 23))";
        int exp_rc            = 1;

        if (!TestBTreeNodeDeleteImplCase(&test_num, &num_passed, test_name,
                size, before, val, after, exp_rc))
            return TestDidntExecute(test_num);
    }
    // Case 4: Leaf borrows a key from a sibling
    {
        const char* test_name = "";
        int size              = 4;
        const char* before    = "(1 2 3 4) 10 (11) 20 (21)";
        int val               = 11;
        const char* after     = "((1 2 3) 4 (10) 20 (21))";
        int exp_rc            = 1;

        if (!TestBTreeNodeDeleteImplCase(&test_num, &num_passed, test_name,
                size, before, val, after, exp_rc))
            return TestDidntExecute(test_num);
    }

    // Case 5: Two leaves get merged
    {
        const char* test_name = "";
        int size              = 3;
        const char* before    = "(1) 10 (11) 20 (21 22 23)";
        int val               = 1;
        const char* after     = "((10 11) 20 (21 22 23))";
        int exp_rc            = 1;

        if (!TestBTreeNodeDeleteImplCase(&test_num, &num_passed, test_name,
                size, before, val, after, exp_rc))
            return TestDidntExecute(test_num);
    }
    // Case 6: Deletion from internal node
    {
        const char* test_name = "";
        int size              = 3;
        const char* before =
            "((1 2) 10 (20)) 100 ((101) 110 (114)) 200 ((201) 210 (211) 220 "
            "(221) 230 (235))";
        int val = 10;
        const char* after =
            "(((1) 2 (20)) 100 ((101) 110 (114)) 200 ((201) 210 (211) 220 "
            "(221) "
            "230 (235)))";
        int exp_rc = 1;

        if (!TestBTreeNodeDeleteImplCase(&test_num, &num_passed, test_name,
                size, before, val, after, exp_rc))
            return TestDidntExecute(test_num);
    }
    // Case 7: val missing from tree
    {
        const char* test_name = "";
        int size              = 3;
        const char* before =
            "((25 26) 50 (75)) 100 ((125) 150 (175)) 200 ((225) 250 (275))";
        int val = 10;
        const char* after =
            "(((25 26) 50 (75)) 100 ((125) 150 (175)) 200 ((225) 250 (275)))";
        int exp_rc = 2;

        if (!TestBTreeNodeDeleteImplCase(&test_num, &num_passed, test_name,
                size, before, val, after, exp_rc))
            return TestDidntExecute(test_num);
    }
    // Case 8: merge right
    {
        const char* test_name = "";
        int size              = 3;
        const char* before =
            "((25) 50 (75)) 100 ((125) 150 (175)) 200 ((225) 250 (275))";
        int val = 75;
        const char* after =
            "(((25 50) 100 (125) 150 (175)) 200 ((225) 250 (275)))";
        int exp_rc = 1;

        if (!TestBTreeNodeDeleteImplCase(&test_num, &num_passed, test_name,
                size, before, val, after, exp_rc))
            return TestDidntExecute(test_num);
    }
    // Case 9: merge left
    {
        const char* test_name = "";
        int size              = 3;
        const char* before =
            "((25) 50 (75)) 100 ((125) 150 (175)) 200 ((225) 250 (275))";
        int val = 25;
        // (225) is null!
        const char* after =
            "(((50 75) 100 (125) 150 (175)) 200 ((225) 250 (275)))";
        int exp_rc = 1;

        if (!TestBTreeNodeDeleteImplCase(&test_num, &num_passed, test_name,
                size, before, val, after, exp_rc))
            return TestDidntExecute(test_num);
    }

    PrintTestStats(num_passed, test_num);
    return 1;
}
