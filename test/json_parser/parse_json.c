#include "./parse_json.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

// JSON Spec: https://www.json.org/json-en.html

off_t get_file_size_stat(const char* filename)
{
    struct stat st;
    if (stat(filename, &st) == 0) return st.st_size;

    return -1;  // Fail
}

typedef enum JsonTokenType
{
    JSON_TOKEN_STRING,
    JSON_TOKEN_NUMBER,
    JSON_TOKEN_COLON,
    JSON_TOKEN_COMMA,
    JSON_TOKEN_LEFT_BRACKET,
    JSON_TOKEN_RBRACKET,
    JSON_TOKEN_LEFT_SQUACKET,
    JSON_TOKEN_RIGHT_SQUACKET,
} JsonTokenType;

typedef struct JsonToken
{
    JsonTokenType type;
    size_t string_start;
    size_t string_len;
    char* data;
    union
    {
        JsonString string;
        JsonNumber number;
    };
} JsonToken;

typedef struct JsonLexSettings
{
    bool make_seq;
} JsonLexSettings;

typedef struct JsonLexState
{
    // Either in a string, in a number, or in nothing
    bool in_string;
    bool in_number;

    // e.g. if we've parsed "\u00", `escaped` is true and `escape_seq_index` is
    // 2
    bool escaped;
    size_t escape_seq_index;

    // Applicable if `in_string` is true. we track `string_len` rather than the
    // end position (e.g. `string_end`) becasue escaped characters may make the
    // actual string shorter than `string_end - string_start`
    size_t string_start;
    size_t string_len;

    // Object depth. Whether or not we've added support for arrays when you're
    // reading this, if we've parsed this far
    //
    //          "{[{ ... }]}"
    //             ^
    //
    // `depth` would be set to 2.
    size_t depth;
    size_t max_depth;

    FILE* fp;
    size_t tok_idx;

    JsonLexSettings settings;

} JsonLexState;

void json_tok_destroy(JsonToken* tok)
{
    if (tok->data != NULL)
    {
        free(tok->data);
    }

    free(tok);
}

static bool lex_state_is_valid(JsonLexState state)
{
    if (!state.in_string && state.escaped)
    {
        printf("In quote AND escaped\n");
        return false;
    }

    if (state.in_number && state.in_string)
    {
        printf("In number AND in quote\n");
        return false;
    }

    if (state.in_number && state.escaped)
    {
        printf("In number AND escaped\n");
        return false;
    }

    return true;
}

static const char* json_tok_type_to_sting(JsonToken tok)
{
    if (tok.type == JSON_TOKEN_STRING) return "String";
    if (tok.type == JSON_TOKEN_NUMBER) return "Number";
    if (tok.type == JSON_TOKEN_COMMA) return "Comma";
    if (tok.type == JSON_TOKEN_COLON) return "Colon";
    if (tok.type == JSON_TOKEN_LEFT_BRACKET) return "Left bracket";
    if (tok.type == JSON_TOKEN_RBRACKET) return "Right bracket";
    if (tok.type == JSON_TOKEN_LEFT_SQUACKET) return "Left square bracket";
    if (tok.type == JSON_TOKEN_RIGHT_SQUACKET) return "Right square bracket";
    return "Null";
}

static const char* json_tok_to_sting(JsonToken tok)
{
    if (tok.type == JSON_TOKEN_STRING || tok.type == JSON_TOKEN_NUMBER)
    {
        static char buff[30];
        memset(buff, '\0', 30 * sizeof(char));

        char* content = tok.data == NULL ? "Null" : tok.data;

        snprintf(buff, 29 * sizeof(char), "%s: %s", json_tok_type_to_sting(tok),
            content);

        return buff;
    }
    else
    {
        return json_tok_type_to_sting(tok);
    }
}

static bool is_ok_in_string(char c) { return true; }

static bool is_number(char c) { return isdigit(c) || c == '+' || c == '-'; }

static bool can_unescaped_char(char unescaped)
{
    // TODO: Support \u####
    return unescaped == 'b' || unescaped == 'f' || unescaped == 'n' ||
           unescaped == 'r' || unescaped == '"' || unescaped == '\\' ||
           unescaped == '/';
}

static bool add_tok(JsonLexState* state,
    JsonToken* seq,
    JsonTokenType tok_type,
    size_t num_toks,
    char** err_msg)
{
    if (state->settings.make_seq)
    {
        if (state->tok_idx == num_toks)
        {
            // Too many tokens
            *err_msg = "Too many tokens!";

            return false;
        }

        seq[state->tok_idx].type = tok_type;
        seq[state->tok_idx].data = NULL;

        if (tok_type == JSON_TOKEN_STRING || tok_type == JSON_TOKEN_NUMBER)
        {
            seq[state->tok_idx].string_len   = state->string_len;
            seq[state->tok_idx].string_start = state->string_start;

            // Save file position
            size_t fpos = ftell(state->fp);
            fseek(state->fp, state->string_start, SEEK_SET);

            char* buff = (char*)calloc(state->string_len + 1, sizeof(char));
            if (buff == NULL)
            {
                *err_msg = "oom";

                return false;
            }

            for (size_t i = 0; i < state->string_len; i++)
            {
                // TODO: Handle escape sequences
                char c = fgetc(state->fp);
                assert(c != EOF);
                buff[i] = c;
            }

            seq[state->tok_idx].data = buff;

            // Go back to current pos
            fseek(state->fp, fpos, SEEK_SET);
        }
    }

    state->tok_idx += 1;

    return true;
}

// This has two modes: count and build. Count determines how many tokens this
// will need, build assumes you know the number of tokens already and tokenizes
// the contents of the file.
bool json_lex(FILE* fp,
    size_t* num_toks_ptr,
    bool make_seq,
    JsonToken** seq_ptr,
    char** err_msg,
    JsonLexState* state_ptr)
{
    JsonLexSettings settings = {
        .make_seq = make_seq,
    };

    JsonLexState state = {
        .in_string    = false,
        .escaped      = false,
        .in_number    = false,
        .string_start = 0,
        .string_len   = 0,
        .depth        = 0,
        .tok_idx      = 0,
        .settings     = settings,
        .fp           = fp,
        .depth        = 0,
        .max_depth    = 0,
    };

    size_t num_toks = 0;

    JsonToken* seq  = NULL;
    if (make_seq)
    {
        // Build token sequence
        num_toks = *num_toks_ptr;
        seq      = (JsonToken*)malloc(num_toks * sizeof(JsonToken));
    }

    // Round 2: Build token sequence
    // TODO: Seek to beginning fo file
    for (char c = fgetc(state.fp); c != EOF; c = fgetc(state.fp))
    {
        if (state.in_string)
        {
            if (state.escaped)
            {
                // Handle escape sequences
                if (state.escape_seq_index > 0)
                {
                    assert(state.escape_seq_index <= 4);

                    if (!isxdigit(c))
                    {
                        *err_msg = "invalid unicode escape sequence";

                        return 0;
                    }

                    state.escape_seq_index += 1;

                    // In a unicode escape sequence
                    if (state.escape_seq_index > 4)
                    {
                        state.escape_seq_index = 0;  // Redundant, but why not
                        state.escaped          = false;
                    }
                }
                else
                {
                    assert(state.escape_seq_index == 0);

                    if (c == 'u')
                    {
                        // Entering a unicode sequence
                        state.escape_seq_index += 1;
                    }
                    else if (can_unescaped_char(c))
                    {
                        state.escaped = false;
                    }
                    else
                    {
                        // Error
                        static char buff[30] = {0};
                        sprintf(buff, "Can't escape character '%c'", c);
                        *err_msg = buff;

                        return false;
                    }
                }

                state.string_len += 1;
            }
            else
            {
                if (c == '"')
                {
                    state.in_string = false;

                    // =========
                    // New token
                    // =========
                    if (!add_tok(
                            &state, seq, JSON_TOKEN_STRING, num_toks, err_msg))
                    {
                        return false;
                    }
                }
                else if (c == '\\')
                {
                    state.escaped = true;
                }
                else
                {
                    if (!is_ok_in_string(c))
                    {
                        // Error
                        return false;
                    }

                    state.string_len += 1;
                }
            }
        }
        else
        {
            if (c == ':')
            {
                // =========
                // New token
                // =========
                if (!add_tok(&state, seq, JSON_TOKEN_COLON, num_toks, err_msg))
                {
                    return false;
                }
            }
            else if (c == ',')
            {
                if (state.in_number)
                {
                    state.in_number = false;

                    if (!add_tok(
                            &state, seq, JSON_TOKEN_NUMBER, num_toks, err_msg))
                    {
                        return 0;
                    }
                }

                // =========
                // New token
                // =========
                if (!add_tok(&state, seq, JSON_TOKEN_COMMA, num_toks, err_msg))
                {
                    return false;
                }
            }
            else if (c == '"')
            {
                state.in_string    = true;
                state.string_start = ftell(state.fp);
                state.string_len   = 0;
            }
            else if (c == '{')
            {
                state.depth += 1;
                if (state.depth > state.max_depth)
                {
                    state.max_depth = state.depth;
                }

                // =========
                // New token
                // =========
                if (!add_tok(&state, seq, JSON_TOKEN_LEFT_BRACKET, num_toks,
                        err_msg))
                {
                    return false;
                }
            }
            else if (c == '}')
            {
                if (state.in_number)
                {
                    state.in_number = false;

                    if (!add_tok(
                            &state, seq, JSON_TOKEN_NUMBER, num_toks, err_msg))
                    {
                        return 0;
                    }
                }

                state.depth -= 1;

                // =========
                // New token
                // =========
                if (!add_tok(
                        &state, seq, JSON_TOKEN_RBRACKET, num_toks, err_msg))
                {
                    return false;
                }
            }
            else if (c == '[')
            {
                // =========
                // New token
                // =========
                if (!add_tok(&state, seq, JSON_TOKEN_LEFT_SQUACKET, num_toks,
                        err_msg))
                {
                    return false;
                }
            }
            else if (c == ']')
            {
                if (state.in_number)
                {
                    state.in_number = false;

                    if (!add_tok(
                            &state, seq, JSON_TOKEN_NUMBER, num_toks, err_msg))
                    {
                        return 0;
                    }
                }

                // =========
                // New token
                // =========
                if (!add_tok(&state, seq, JSON_TOKEN_RIGHT_SQUACKET, num_toks,
                        err_msg))
                {
                    return false;
                }
            }
            else if (is_number(c))
            {
                // We'll validate the formatting later on
                if (!state.in_number)
                {
                    state.in_number = true;
                    size_t fpos     = ftell(state.fp);
                    assert(fpos > 0);
                    state.string_start = fpos - 1;
                    state.string_len   = 0;
                }

                state.string_len += 1;
            }
            else if (isspace(c))
            {
                if (state.in_number)
                {
                    state.in_number = false;

                    // =========
                    // New token
                    // =========
                    if (!add_tok(
                            &state, seq, JSON_TOKEN_NUMBER, num_toks, err_msg))
                    {
                        return false;
                    }
                }
            }
            else
            {
                // Error
                static char buff[30] = {0};
                sprintf(buff, "Unexpected character '%c'", c);
                *err_msg = buff;

                return false;
            }
        }

        // Validate state for debugging
        assert(lex_state_is_valid(state));
    }

    if (make_seq)
    {
        *seq_ptr = seq;
    }
    else
    {
        *num_toks_ptr = state.tok_idx;
    }

    *state_ptr = state;

    return true;
}

int hex_to_nibble(char c)
{
    int d = 0;
    if (isalpha(c))
    {
        d |= (toupper(c) - 'A');
    }
    else
    {
        d |= (c - '0');
    }
    return d;
}

bool parse_json_string(const char* escaped_str,
    char* str,
    size_t escaped_str_len,
    size_t len,
    char** err_msg)
{
    size_t escape_seq_index = 0;
    size_t str_index        = 0;
    int unescaped_char      = 0;

    for (size_t escaped_str_index = 0; escaped_str_index < escaped_str_len;
        escaped_str_index++)
    {
        char c = escaped_str[escaped_str_index];

        if (escape_seq_index > 0)
        {
            if (escape_seq_index == 1)
            {
                if (c == 'u')
                {
                    escape_seq_index += 1;
                }
                else
                {
                    escape_seq_index = 0;

                    if (c == 'b')
                    {
                        str[str_index] = '\b';
                    }
                    else if (c == 'f')
                    {
                        str[str_index] = '\f';
                    }
                    else if (c == 'n')
                    {
                        str[str_index] = '\n';
                    }
                    else if (c == 'r')
                    {
                        str[str_index] = '\r';
                    }
                    else if (c == 't')
                    {
                        // Technically not part of the JSON spec, but useful for
                        // testing
                        str[str_index] = '\t';
                    }
                    else if (c == '"')
                    {
                        str[str_index] = '"';
                    }
                    else if (c == '\\')
                    {
                        str[str_index] = '\\';
                    }
                    else if (c == '/')
                    {
                        str[str_index] = '/';
                    }
                    else
                    {
                        // Error
                        *err_msg = "Bad escape sequence";
                        return 0;
                    }

                    str_index += 1;
                }
            }
            else
            {
                // Keep processing this sequence
                if (isxdigit(c))
                {
                    unescaped_char <<= 4;
                    unescaped_char |= hex_to_nibble(c);
                }
                else
                {
                    // Error
                    *err_msg = "Bad unicode escape sequence";
                    return 0;
                }

                if (escape_seq_index < 5)
                {
                    escape_seq_index += 1;
                }
                else
                {
                    // Unescape

                    if (unescaped_char < CHAR_MIN || unescaped_char > CHAR_MAX)
                    {
                        // UTF-16 not yet supported. Eventually we'll deduce
                        // whether a wider char type is needed during the first
                        // pass, then use a wchar array if needed.
                        *err_msg = "Unsupported UTF-16 character";
                        return 0;
                    }
                    else
                    {
                        str[str_index] = (char)unescaped_char;

                        str_index += 1;

                        escape_seq_index = 0;
                        unescaped_char   = 0;
                    }
                }
            }
        }
        else
        {
            if (c == '\\')
            {
                escape_seq_index = 1;
            }
            else
            {
                str[str_index] = c;

                str_index += 1;
            }
        }
    }

    // Last character checks
    char c = escaped_str[escaped_str_len - 1];

    if (c == 'u' && escaped_str_len > 1 &&
        escaped_str[escaped_str_len - 2] == '\\')
    {
        *err_msg = "Invalid escape sequence \"\\u\"";

        return false;
    }

    if (escape_seq_index > 0)
    {
        *err_msg == "Escaped string ended at a single backslash";

        return false;
    }

    return true;
}

bool isonenine(char c) { return isdigit(c) && c != '0'; }

/**
 * @brief Parse a string and determine if it is a number according to JSON spec.
 * Sets the `is_integer` field of `number` if so.
 *
 * @param num_str Pointer to data that we suspect represents a number
 * @param len Length of `num_str`
 * @param number JsonNumber object
 * @param err_msg Error message populated on failure
 * @return True if the data represents a number according to JSON spec, false
 * otherwise
 */
bool parse_json_number_validate_only(
    const char* num_str, size_t len, JsonNumber* number, const char** err_msg)
{
    bool is_integer = true;

    size_t i        = 0;
    char c          = num_str[i];

    // integer
    if (c == '-')
    {
        i += 1;
    }
    else if (c == '0')
    {
        i += 1;

        if (i < len)
        {
            c = num_str[i];

            if (c == '.')
            {
                // 0e#
                i += 1;
                goto fraction;
            }
            else if (c == 'e' | c == 'E')
            {
                // 0.#
                i += 1;
                goto exponent;
            }
            else
            {
                // Fail
                *err_msg = "invalid character after leading '0'";
                return false;
            }
        }
    }
    else if (isdigit(c))
    {
        // 1-9
        i += 1;

        while (i < len)
        {
            c = num_str[i];

            if (isdigit(c))
            {
                // 1234
                i += 1;
            }
            else if (c == '.')
            {
                // 1234.
                i += 1;
                goto fraction;
            }
            else if (c == 'e' | c == 'E')
            {
                // 1234e
                i += 1;
                goto exponent;
            }
            else
            {
                // Fail
                *err_msg = "invalid character after integer part";
                return false;
            }
        }
    }
    else
    {
        // Fail
        *err_msg = "invalid first character in number characer sequence";
        return false;
    }

    goto success;

fraction:
    if (i < len)
    {
        c = num_str[i];

        // We can only end up here if we encountered a '.' and aren't done yet.
        is_integer = false;

        if (isdigit(c))
        {
            i += 1;
        }
        else
        {
            // fail
            *err_msg = "invalid character after '.' (non-digit)";
            return false;
        }

        while (i < len)
        {
            c = num_str[i];

            if (isdigit(c))
            {
                i += 1;
            }
            else if (c == 'e' || c == 'E')
            {
                i += 1;
                goto exponent;
            }
        }
    }
    else
    {
        // Fail: "1234."
        *err_msg = "number ended in '.'";
        return false;
    }

    goto success;

    // exponent
exponent:
    if (i < len)
    {
        c = num_str[i];

        if (c == '-' || c == '+')
        {
            i += 1;

            if (i < len)
            {
                c = num_str[i];

                if (isdigit(c))
                {
                    i += 1;
                }
                else
                {
                    // Fail: "1234e$"
                    *err_msg = "invalid character after exponent ('e')";
                    return false;
                }
            }
            else
            {
                // Fail: "1234e-"
                *err_msg = "number ended after exponent sign (e+|-)";
                return false;
            }
        }

        while (i < len)
        {
            c = num_str[i];

            if (isdigit(c))
            {
                i += 1;
            }
            else
            {
                // Fail
                *err_msg = "non-digit encountered in exponent";
                return false;
            }
        }
    }
    else
    {
        // Fail: "1234e"
        *err_msg = "number ended at exponent symbol (e|E)";
        return false;
    }

success:

    number->is_integer = is_integer;

    return true;
}

// This one assumes `len` is redundant and that byte `len` after `num_str` is
// a null character.
bool parse_json_number(
    const char* num_str, size_t len, JsonNumber* number, char** err_msg)
{
    if (!parse_json_number_validate_only(num_str, len, number, err_msg))
    {
        return false;
    }

    // Store the number
    if (number->is_integer)
    {
        char* first_invalid = NULL;
        number->integer     = strtol(num_str, &first_invalid, 10);
    }
    else
    {
        char* first_invalid = NULL;
        number->fraction    = strtod(num_str, &first_invalid);
    }

    return true;
}

// What fun! Implement strtof and atoi yourself!
bool parse_json_number_experimental(const char* num_str,
    size_t len,
    long* num_int,
    double* num_float,
    bool* is_int,
    const char** err_msg)
{
    size_t i = 0;
    char c   = num_str[i];

    int sign = 1;
    int int_part;
    int frac_exp = 10;
    int frac_part;
    int exp_sign = 1;
    int exp_part;
    int base = 10;

    // integer
    if (c == '-')
    {
        sign = -1;
        i += 1;
    }
    else if (c == '0')
    {
        i += 1;

        if (i < len)
        {
            c = num_str[i];

            if (c == '.')
            {
                // 0e#
                i += 1;
                goto fraction;
            }
            else if (c == 'e' | c == 'E')
            {
                // 0.#
                i += 1;
                goto exponent;
            }
            else
            {
                // Fail
                return false;
            }
        }
    }
    else if (isdigit(c))
    {
        int_part += (c - '0');

        // 1-9
        i += 1;

        while (i < len)
        {
            c = num_str[i];

            if (isdigit(c))
            {
                int_part *= base;
                int_part += (c - '0');
                // 1234
                i += 1;
            }
            else if (c == '.')
            {
                // 1234.
                i += 1;
                goto fraction;
            }
            else if (c == 'e' | c == 'E')
            {
                // 1234e
                i += 1;
                goto exponent;
            }
            else
            {
                // Fail
                return false;
            }
        }
    }

fraction:
    if (i < len)
    {
        c = num_str[i];

        if (isdigit(c))
        {
            i += 1;
        }
        else
        {
            // fail
            return false;
        }

        while (i < len)
        {
            c = num_str[i];

            if (isdigit(c))
            {
                i += 1;
            }
            else if (c == 'e' || c == 'E')
            {
                i += 1;
                goto exponent;
            }
        }
    }
    else
    {
        // Fail: "1234."
        return false;
    }

    // exponent
exponent:
    if (i < len)
    {
        c = num_str[i];

        if (i == '-' || i == '+')
        {
            i += 1;

            if (i < len)
            {
                c = num_str[i];

                if (isdigit(c))
                {
                    i += 1;
                }
                else
                {
                    // Fail: "1234e$"
                    return false;
                }
            }
            else
            {
                // Fail: "1234e-"
                return false;
            }
        }

        while (i < len)
        {
            c = num_str[i];

            if (isdigit(c))
            {
                i += 1;
            }
            else
            {
                // Fail
                return false;
            }
        }
    }
    else
    {
        // Fail: "1234e"
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
/// JSON OBJECT BUILDER                                                      ///
////////////////////////////////////////////////////////////////////////////////

typedef enum JsonState
{
    // Allowed tokens: left bracket, left squacket
    PARSE_STATE_INITIAL_STATE,
    // Allowed tokens: string (property name), right bracket
    PARSE_STATE_OBJECT_AFTER_LEFT_BRACKET,
    // Allowed tokens: colon
    PARSE_STATE_OBJECT_AFTER_PROPERTY_NAME,
    // Allowed tokens: left bracket, left squacket, string, number
    PARSE_STATE_OBJECT_AFTER_COLON,
    // Allowed tokens: comma, right bracket
    PARSE_STATE_OBJECT_AFTER_VALUE,
    // Allowed tokens: string (property name)
    PARSE_STATE_OBJECT_AFTER_COMMA,
    // Allowed tokens: string, number, left bracket, left squacket, right
    // squacket
    PARSE_STATE_LIST_AFTER_LEFT_BRACKET,
    // Allowed tokens: comma, right squacket
    PARSE_STATE_LIST_AFTER_VALUE,
    // Allowed tokens: string, number, left bracket, left squacket
    PARSE_STATE_LIST_AFTER_COMMA,
    // Allowed tokens: -
    PARSE_STATE_FAIL,
    // Should only be encountered when last token is consumed
    PARSE_STATE_OUT_OF_ROOT,
} JsonState;

const char* parse_state_to_string(JsonState state)
{
    if (state == PARSE_STATE_INITIAL_STATE)
    {
        return "Initial State";
    }
    if (state == PARSE_STATE_OUT_OF_ROOT)
    {
        return "Out of Root";
    }
    if (state == PARSE_STATE_FAIL)
    {
        return "Failure";
    }
    if (state == PARSE_STATE_OBJECT_AFTER_COLON)
    {
        return "Object, After Colon";
    }
    if (state == PARSE_STATE_OBJECT_AFTER_COMMA)
    {
        return "Object, After Comma";
    }
    if (state == PARSE_STATE_OBJECT_AFTER_LEFT_BRACKET)
    {
        return "Object, After Left Bracket";
    }
    if (state == PARSE_STATE_OBJECT_AFTER_PROPERTY_NAME)
    {
        return "Object, After Property Name";
    }
    if (state == PARSE_STATE_OBJECT_AFTER_VALUE)
    {
        return "Object, After Value";
    }
    if (state == PARSE_STATE_LIST_AFTER_COMMA)
    {
        return "List, After Comma";
    }
    if (state == PARSE_STATE_LIST_AFTER_LEFT_BRACKET)
    {
        return "List, After Left Bracket";
    }
    if (state == PARSE_STATE_LIST_AFTER_VALUE)
    {
        return "List, After Value";
    }
    return "Undefined";
}

bool inc_list_size(JsonList* list)
{
    size_t num_values = list->num_values;
    JsonValue* values = list->values;

    if (num_values == 0)
    {
        // Initialize the list
        values = (JsonValue*)malloc(sizeof(JsonValue));
    }
    else
    {
        // Resize the list
        assert(values != NULL);

        values =
            (JsonValue*)realloc(values, (num_values + 1) * sizeof(JsonValue));
    }

    if (values == NULL)
    {
        return false;
    }

    list->num_values += 1;
    list->values = values;
}

bool inc_obj_size(JsonObject* object)
{
    size_t num_properties    = object->num_properties;
    JsonProperty* properties = object->properties;

    if (num_properties == 0)
    {
        // Initialize the list
        properties = (JsonProperty*)malloc(sizeof(JsonProperty));
    }
    else
    {
        // Resize the list
        assert(properties != NULL);

        properties = (JsonProperty*)realloc(
            properties, (num_properties + 1) * sizeof(JsonProperty));
    }

    if (properties == NULL)
    {
        return false;
    }

    object->num_properties += 1;
    object->properties = properties;

    return true;
}

JsonState get_after_value_state(JsonCursor* path_stack, size_t path_stack_size)
{
    if (path_stack_size == 0)
    {
        return PARSE_STATE_OUT_OF_ROOT;
    }

    JsonCursorType curr_cursor_type = path_stack[path_stack_size - 1].type;

    if (curr_cursor_type == JSON_CURSOR_TYPE_OBJECT)
    {
        return PARSE_STATE_OBJECT_AFTER_VALUE;
    }
    else if (curr_cursor_type == JSON_CURSOR_TYPE_LIST)
    {
        return PARSE_STATE_LIST_AFTER_VALUE;
    }
    else
    {
        return PARSE_STATE_FAIL;
    }
}

JsonObject* new_object() { return (JsonObject*)malloc(sizeof(JsonObject)); }
JsonList* new_list() { return (JsonList*)malloc(sizeof(JsonList)); }

/**
 * @brief Parse a JSON file
 *
 * @param fp pointer to the file
 * @param settings parser settings object
 * @param root_object[out] root of the JSON structure if the root is an
 * object
 * @param root_list[out] root of the JSON structure if the root is a list
 * @param err_msg[out] error message populated on failure
 * @return true on success, false on failure
 */
bool parse_json(FILE* fp,
    JsonSettings settings,
    JsonObject** root_object_ptr,
    JsonList** root_list_ptr,
    char** err_msg)
{
    // LEXER: First pass
    size_t num_toks = 0;
    JsonLexState lex_state;
    if (!json_lex(fp, &num_toks, false, NULL, err_msg, &lex_state))
    {
        return false;
    }

    // LEXER: Second pass
    rewind(fp);
    JsonToken* seq = NULL;
    if (!json_lex(fp, &num_toks, true, &seq, err_msg, &lex_state))
    {
        return false;
    }

    if (seq == NULL)
    {
        *err_msg = "oom";
        return NULL;
    }

    // PARSER: First pass
    size_t path_stack_size     = 0;
    size_t path_stack_capacity = lex_state.max_depth;
    JsonCursor* path_stack =
        (JsonCursor*)malloc(path_stack_capacity * sizeof(JsonCursor));

    if (path_stack == NULL)
    {
        *err_msg = "oom";
        free(seq);
        return NULL;
    }

    JsonState state = PARSE_STATE_INITIAL_STATE;

    for (size_t i = 0; i < num_toks; i++)
    {
        printf("State: %s\n", parse_state_to_string(state));

        /* Unpack */
        JsonToken tok      = seq[i];
        JsonTokenType type = tok.type;
        JsonCursor current_cursor;

        if (path_stack_size > 0)
        {
            current_cursor = path_stack[path_stack_size - 1];
        }

        /* State transition */
        if (state == PARSE_STATE_INITIAL_STATE)
        {
            state = PARSE_STATE_FAIL;

            path_stack_size += 1;
            assert(path_stack_size <= path_stack_capacity);

            // clang-format off
            if (type == JSON_TOKEN_LEFT_BRACKET)
            {
                path_stack[path_stack_size - 1] = (JsonCursor) {
                    .type = JSON_CURSOR_TYPE_OBJECT,
                    .object = new_object(),
                };
                
                if (path_stack[path_stack_size - 1].object != NULL) 
                {
                    state = PARSE_STATE_OBJECT_AFTER_LEFT_BRACKET;
                }
            }
            else if (type == JSON_TOKEN_LEFT_SQUACKET)
            {
                path_stack[path_stack_size - 1] = (JsonCursor) {
                    .type = JSON_CURSOR_TYPE_LIST,
                    .list = new_list(),
                };
                 
                if (path_stack[path_stack_size - 1].list != NULL) 
                {
                    state = PARSE_STATE_LIST_AFTER_LEFT_BRACKET;
                }
            }
            else
            {
                *err_msg = "(lexer) invalid first token. Must be an opening bracket such as '[' or '{'";
            }
            // clang-format on
        }
        else if (state == PARSE_STATE_OBJECT_AFTER_LEFT_BRACKET)
        {
            state = PARSE_STATE_FAIL;

            assert(current_cursor.type == JSON_CURSOR_TYPE_OBJECT);

            JsonObject* current_object = current_cursor.object;

            if (type == JSON_TOKEN_STRING && inc_obj_size(current_object))
            {
                JsonProperty* properties = current_object->properties;
                size_t num_properties    = current_object->num_properties;

                assert(properties != NULL);
                assert(num_properties > 0);

                JsonProperty last_property = properties[num_properties - 1];
                last_property.name         = tok.string;

                state = PARSE_STATE_OBJECT_AFTER_PROPERTY_NAME;
            }
            else if (type == JSON_TOKEN_RBRACKET)
            {
                path_stack_size -= 1;

                state = get_after_value_state(path_stack, path_stack_size);
            }
            else
            {
                // TODO: Fix error message
                *err_msg = "bad syntax";
            }
        }
        else if (state == PARSE_STATE_OBJECT_AFTER_PROPERTY_NAME)
        {
            state = PARSE_STATE_FAIL;

            if (type == JSON_TOKEN_COLON)
            {
                state = PARSE_STATE_OBJECT_AFTER_COLON;
            }
            else
            {
                // TODO: Fix error message
                *err_msg = "bad syntax";
            }
        }
        else if (state == PARSE_STATE_OBJECT_AFTER_COLON)
        {
            state = PARSE_STATE_FAIL;

            assert(current_cursor.type == JSON_CURSOR_TYPE_OBJECT);

            JsonObject* current_object = current_cursor.object;

            assert(current_object != NULL);

            JsonProperty* properties = current_object->properties;
            size_t num_properties    = current_object->num_properties;

            assert(properties != NULL);
            assert(num_properties > 0);

            if (type == JSON_TOKEN_STRING)
            {
                properties[num_properties - 1].value = (JsonValue){
                    .type   = JSON_STRING,
                    .string = tok.string,
                };

                // Success
                state = PARSE_STATE_OBJECT_AFTER_VALUE;
            }
            else if (type == JSON_TOKEN_NUMBER)
            {
                properties[num_properties - 1].value = (JsonValue){
                    .type   = JSON_NUMBER,
                    .number = tok.number,
                };
            }
            else if (type == JSON_TOKEN_LEFT_BRACKET)
            // clang-format off
            {
                properties[num_properties - 1].value = (JsonValue){
                    .type = JSON_OBJECT,
                };
                
                JsonObject* new_object = &(properties[num_properties - 1].value.object);

                // Push new cursor onto the path stack, add the new OBJECT
                // to it, and step into it
                path_stack_size += 1;
                JsonCursor new_cursor = {
                    .type = JSON_CURSOR_TYPE_OBJECT,
                    .object = new_object,
                };

                path_stack[path_stack_size - 1] = new_cursor;

                // Success: We're in an OBJECT
                state = PARSE_STATE_OBJECT_AFTER_LEFT_BRACKET;
            }
            else if (type == JSON_TOKEN_LEFT_SQUACKET)
            {
                properties[num_properties - 1].value = (JsonValue){
                    .type = JSON_LIST,
                };
                
                JsonList* new_list = &(properties[num_properties - 1].value.list);

                // Push new cursor onto the path stack, add the new OBJECT
                // to it, and step into it
                path_stack_size += 1;
                JsonCursor new_cursor = {
                    .type = JSON_CURSOR_TYPE_LIST,
                    .list = new_list,
                };


                path_stack[path_stack_size - 1] = new_cursor;

                // Success: We're in a LIST
                state = PARSE_STATE_LIST_AFTER_LEFT_BRACKET;
            }
            // clang-format on
            else
            {
                // TODO: Fix error message
                *err_msg = "bad syntax";
            }
        }
        else if (state == PARSE_STATE_OBJECT_AFTER_COMMA)
        {
            state = PARSE_STATE_FAIL;

            assert(current_cursor.type == JSON_CURSOR_TYPE_OBJECT);

            JsonObject* current_object = current_cursor.object;

            if (type == JSON_TOKEN_STRING && inc_obj_size(current_object))
            {
                JsonProperty* properties = current_object->properties;
                size_t num_properties    = current_object->num_properties;

                assert(properties != NULL);
                assert(num_properties > 0);

                properties[num_properties - 1].name = tok.string;

                state = PARSE_STATE_OBJECT_AFTER_PROPERTY_NAME;
            }
            else
            {
                // TODO: Fix error message
                *err_msg = "bad syntax";
            }
        }
        else if (state == PARSE_STATE_OBJECT_AFTER_VALUE)
        {
            state = PARSE_STATE_FAIL;

            if (type == JSON_TOKEN_COMMA)
            {
                state = PARSE_STATE_OBJECT_AFTER_COMMA;
            }
            else if (type == JSON_TOKEN_RBRACKET)
            {
                path_stack_size -= 1;

                state = get_after_value_state(path_stack, path_stack_size);
            }
            else
            {
                // TODO: Fix error message
                *err_msg = "bad syntax";
            }
        }
        else if (state == PARSE_STATE_LIST_AFTER_COMMA)
        {
            state = PARSE_STATE_FAIL;

            assert(current_cursor.type == JSON_CURSOR_TYPE_LIST);

            JsonList* current_list = current_cursor.list;

            assert(current_list != NULL);

            JsonValue* values = current_list->values;
            size_t num_values = current_list->num_values;

            assert(values != NULL);
            assert(num_values > 0);

            // clang-format off
            if (type == JSON_TOKEN_STRING && inc_list_size(&current_list))
            {
                values[num_values - 1] = (JsonValue){
                    .type   = JSON_STRING,
                    .string = tok.string,
                };

                // Success
                state = PARSE_STATE_LIST_AFTER_VALUE;
            }
            else if (type == JSON_TOKEN_NUMBER && inc_list_size(&current_list))
            {
                values[num_values - 1] = (JsonValue){
                    .type   = JSON_NUMBER,
                    .number = tok.number,
                };

                // Success
                state = PARSE_STATE_LIST_AFTER_VALUE;
            }
            else if (type == JSON_TOKEN_LEFT_BRACKET && inc_list_size(&current_list))
            {
                values[num_values - 1] = (JsonValue){
                    .type = JSON_OBJECT,
                };

                // Push new cursor onto the path stack, add the new object
                // to it, and step into it
                path_stack_size += 1;
                JsonCursor new_cursor = {
                    .type   = JSON_CURSOR_TYPE_OBJECT,
                    .object = &(values[num_values - 1].object),
                };
                path_stack[path_stack_size - 1] = new_cursor;

                // Success: We're in an object
                state = PARSE_STATE_OBJECT_AFTER_LEFT_BRACKET;

            }
            else if (type == JSON_TOKEN_LEFT_SQUACKET && inc_list_size(&current_list)) {
                values[num_values - 1] = (JsonValue){
                    .type = JSON_LIST,
                };

                // Push new cursor onto the path stack, add the new LIST
                // to it, and step into it
                path_stack_size += 1;
                JsonCursor new_cursor = {
                    .type = JSON_CURSOR_TYPE_LIST,
                    .list = &(values[num_values - 1].object),
                };
                path_stack[path_stack_size - 1] = new_cursor;

                // Success: We're in a list
                state = PARSE_STATE_LIST_AFTER_LEFT_BRACKET;
            }
            else
            {
                // TODO: Fix error message
                *err_msg = "bad syntax";
            }
            // clang-format on
        }
        else if (state == PARSE_STATE_LIST_AFTER_LEFT_BRACKET)
        {
            state = PARSE_STATE_FAIL;

            assert(current_cursor.type == JSON_CURSOR_TYPE_LIST);

            JsonList* current_list = current_cursor.list;

            assert(current_list != NULL);

            JsonValue* values = current_list->values;
            size_t num_values = current_list->num_values;

            assert(values != NULL);
            assert(num_values > 0);

            if (type == JSON_TOKEN_STRING)
            {
                if (inc_list_size(&current_list))
                {
                    values[num_values - 1] = (JsonValue){
                        .type   = JSON_STRING,
                        .string = tok.string,
                    };

                    // Success
                    state = PARSE_STATE_LIST_AFTER_VALUE;
                }
            }
            else if (type == JSON_TOKEN_NUMBER)
            {
                if (inc_list_size(&current_list))
                {
                    values[num_values - 1] = (JsonValue){
                        .type   = JSON_NUMBER,
                        .number = tok.number,
                    };

                    // Success
                    state = PARSE_STATE_LIST_AFTER_VALUE;
                }
            }
            else if (type == JSON_TOKEN_LEFT_BRACKET)
            {
                if (inc_list_size(&current_list))
                {
                    values[num_values - 1] = (JsonValue){
                        .type = JSON_OBJECT,
                    };

                    // Push new cursor onto the path stack, add the new object
                    // to it, and step into it
                    path_stack_size += 1;
                    path_stack[path_stack_size - 1] = (JsonCursor){
                        .type   = JSON_CURSOR_TYPE_OBJECT,
                        .object = &(values[num_values - 1].object),
                    };

                    // Success: We're in an object
                    state = PARSE_STATE_OBJECT_AFTER_LEFT_BRACKET;
                }
            }
            else if (type == JSON_TOKEN_LEFT_SQUACKET)
            {
                if (inc_list_size(&current_list))
                {
                    values[num_values - 1] = (JsonValue){
                        .type = JSON_LIST,
                    };

                    // Push new cursor onto the path stack, add the new LIST
                    // to it, and step into it
                    path_stack_size += 1;
                    path_stack[path_stack_size - 1] = (JsonCursor){
                        .type = JSON_CURSOR_TYPE_LIST,
                        .list = &(values[num_values - 1].object),
                    };

                    // Success: We're in a list
                    state = PARSE_STATE_LIST_AFTER_LEFT_BRACKET;
                }
            }
            else if (type == JSON_TOKEN_RIGHT_SQUACKET)
            {
                path_stack_size -= 1;

                state = get_after_value_state(path_stack, path_stack_size);
            }
            else
            {
                // TODO: Fix error message
                *err_msg = "bad syntax";
            }
        }
        else if (state == PARSE_STATE_LIST_AFTER_VALUE)
        {
            state = PARSE_STATE_FAIL;

            if (type == JSON_TOKEN_COMMA)
            {
                state = PARSE_STATE_LIST_AFTER_COMMA;
            }
            else if (type == JSON_TOKEN_RIGHT_SQUACKET)
            {
                path_stack_size -= 1;

                state = get_after_value_state(path_stack, path_stack_size);
            }
            else if (PARSE_STATE_OUT_OF_ROOT)
            {
                *err_msg = "out of root too early";
            }
            else
            {
                *err_msg = "(internal) unknown parse state - where am I?";
            }
        }
        else if (state == PARSE_STATE_FAIL)
        {
            state = PARSE_STATE_FAIL;

            // TODO: Clean upmemory
            return false;
        }
        else
        {
            // TODO: Fix error message
            *err_msg = "(internal) unknown state";
            state    = PARSE_STATE_FAIL;
        }
    }

    // Should be out of root now
    if (state != PARSE_STATE_OUT_OF_ROOT)
    {
        // TODO: Amend this error message
        *err_msg = "finished parsing before root object closed";

        // TODO: Clean up memory
        return false;
    }

    // TODO: Clean up memory
    // TODO: Set root object/list
    return true;
}