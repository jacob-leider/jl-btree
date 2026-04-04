
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../json_parser/parse_json.h"

void file_read_example()
{
    // Test: file reading for-loop
    FILE* fp = fopen("../build_test.h", "r");

    size_t i = 0;
    for (char c = fgetc(fp); c != EOF; c = fgetc(fp))
    {
        i++;
        if (ftell(fp) != i)
        {
            printf("Error: file offset expected to be %d, measured as %d\n", i,
                ftell(fp));
        }
        printf("%c", c);
    }
    printf("\n");
}

void parse_num_and_print_result(char* num_str, bool exp_success)
{
    JsonNumber number = {
        .fraction   = 0,
        .integer    = 0,
        .is_integer = false,
    };
    char* err_msg = NULL;

    if (!parse_json_number(num_str, strlen(num_str), &number, &err_msg))
    {
        if (exp_success)
        {
            printf("Encountered error parsing number: %s\n",
                err_msg == NULL ? "Null" : err_msg);
        }
        else
        {
            printf("Parse failed as expected\n");
        }
    }
    else
    {
        if (exp_success)
        {
            if (number.is_integer)
            {
                printf("Parsed integer: %lld\n", number.integer);
            }
            else
            {
                printf("Parsed double: %f\n", number.fraction);
            }
        }
        else
        {
            printf("Error: Expected failure. parsed number\n");
        }
    }
}

void parse_num_example()
{  // parse a simple integer
    struct Case
    {
        char* str;
        bool valid;
    };

    struct Case cases[] = {
        {"1234",      true },
        {"1234.5678", true },
        {"0e10",      true },
        {"012",       false},
        {"0.12",      true },
        {"00.12",     false},
        {".12",       false},
        {"0.5e3",     true },
        {"0.5e+3",    true },
        {"0.5e-3",    true },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(struct Case); i++)
    {
        parse_num_and_print_result(cases[i].str, cases[i].valid);
    }
}

void parse_string_and_print_result(
    char* escaped_str, char* exp_str, bool exp_success)
{
    char* err_msg  = NULL;
    char buff[100] = {0};  // Should be enough
    memset(buff, 0, 100);

    if (!parse_json_string(
            escaped_str, buff, strlen(escaped_str), strlen(exp_str), &err_msg))
    {
        if (exp_success)
        {
            printf("Encountered error parsing string: %s\n",
                err_msg == NULL ? "Null" : err_msg);
        }
        else
        {
            printf("Parse failed as expected\n");
        }
    }
    else
    {
        if (exp_success)
        {
            if (strncmp(exp_str, buff, strlen(exp_str)))
            {
                printf(
                    "Fail: Parsed string \"%s\" different than expected string "
                    "\"%s\"\n",
                    buff, exp_str);
            }
            else
            {
                printf("Parsed string: %s\n", buff);
            }
        }
        else
        {
            printf("Error: Expected failure. parsed number\n");
        }
    }
}

void parse_string_example()
{  // parse a simple integer
    struct Case
    {
        char* escaped_str;
        char* str;
        bool valid;
    };

    struct Case cases[] = {
        {"abc",                   "abc",        true },
        {"a\\tb",                 "a\tb",       true },
        {"a\\q",                  "",           false},
        {"A\\u0042C",             "ABC",        true },
        {"\\u0041\\u0042\\u0043", "ABC",        true },
        {"\\r\\t\\n\\b\"",        "\r\t\n\b\"", true },
        {"\\u",                   "",           false},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(struct Case); i++)
    {
        parse_string_and_print_result(
            cases[i].escaped_str, cases[i].str, cases[i].valid);
    }
}

typedef struct Object
{
    union
    {
        int integer;
        float fraction;
    };
} Object;

int main()
{
    Object* object   = NULL;
    JsonList* list   = NULL;
    JsonValue* value = NULL;
    FILE* fp         = fopen("../cases/insert/test1.json", "r");
    char* err_msg    = NULL;
    JsonSettings parse_settings;

    if (!parse_json(fp, parse_settings, &value, &err_msg))
    {
        printf("Error: %s\n", err_msg == NULL ? "No error message" : err_msg);
        return 1;
    }

    return 0;
}