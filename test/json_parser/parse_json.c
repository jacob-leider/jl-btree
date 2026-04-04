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
    JSON_TOKEN_RIGHT_BRACKET,
    JSON_TOKEN_LEFT_SQUACKET,
    JSON_TOKEN_RIGHT_SQUACKET,
} JsonTokenType;

typedef enum JsonParentType
{
    JSON_PARENT_TYPE_OBJECT,
    JSON_PARENT_TYPE_LIST,
} JsonParentType;

typedef struct JsonToken
{
    JsonTokenType type;

    // For object tokens
    size_t num_properties;

    // For list tokens
    size_t num_values;

    // For both
    size_t mem_offset;

    // For string/number tokens
    size_t string_start;
    size_t string_len;
    char* data;

    // TODO: This should be computed/done in the parser
    union
    {
        JsonString string;
        JsonNumber number;
    };

    // Index of the left-bracket or left-squacket that opens the deepest value
    // containing this token
    size_t parent_token_index;
} JsonToken;

typedef struct JsonParserMemoryObject
{
    // Tokens
    size_t num_tokens;
    JsonToken* tokens_mem;

    // Properties
    size_t num_properties;
    JsonProperty* properties_mem;
    size_t properties_offset;

    // List values
    size_t num_values;
    JsonValue* values_mem;
    size_t values_offset;

    // Path stack
    size_t path_stack_capacity;
    JsonCursor* path_stack_mem;
} JsonParserMemoryObject;

typedef struct JsonLexerSettings
{
    bool make_seq;
} JsonLexerSettings;

typedef struct JsonLexerInfo
{
    JsonLexerSettings settings;
    size_t max_depth;
    JsonParserMemoryObject* mem_obj;
} JsonLexerInfo;

typedef struct JsonLexerState
{
    // Either in a string, in a number, or in nothing
    bool in_string;
    bool in_number;

    // Parent is a list
    JsonParentType* parent_type_stack;
    size_t parent_type_stack_size;
    size_t parent_type_stack_capacity;

    // e.g. if we've parsed "\u00", `escaped` is true and `escape_seq_index` is
    // 2
    bool escaped;
    size_t escape_seq_index;

    // Applicable if `in_string` is true. we track `string_len` rather than the
    // end position (e.g. `string_end`) becasue escaped characters may make the
    // actual string shorter than `string_end - string_start`
    size_t string_start;
    size_t string_len;

    // Object depth.
    // e.g. If we've parsed this far
    //
    //          "{[{ ... }]}"
    //             ^
    //
    // `depth` would be set to 3.
    size_t depth;
    size_t max_depth;

    // Index of the last '{' or '[' we encountered
    size_t last_object_or_list_token_index;

    FILE* fp;
    size_t token_index;

    JsonLexerSettings settings;

} JsonLexerState;

void json_tok_destroy(JsonToken* tok)
{
    if (tok->data != NULL)
    {
        free(tok->data);
    }

    free(tok);
}

static bool lex_state_is_valid(JsonLexerState state)
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
    if (tok.type == JSON_TOKEN_RIGHT_BRACKET) return "Right bracket";
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

static bool add_tok(JsonLexerState* state,
    JsonToken* seq,
    JsonTokenType tok_type,
    JsonLexerInfo* info,
    char** err_msg)
{
    if (info->settings.make_seq)
    {
        if (state->token_index == info->mem_obj->num_tokens)
        {
            // Too many tokens
            *err_msg = "Too many tokens!";

            return false;
        }

        JsonToken* current_token = seq + state->token_index;

        current_token->type      = tok_type;
        current_token->data      = NULL;

        // Compute parent token
        size_t prev_depth         = 0;
        bool found_parent         = false;
        bool parent_is_list       = false;
        size_t parent_token_index = 0;
        for (size_t offset = 0; offset < state->token_index; offset++)
        {
            JsonToken* prev = seq + (state->token_index - 1 - offset);

            if (prev->type == JSON_TOKEN_RIGHT_BRACKET ||
                JSON_TOKEN_RIGHT_SQUACKET)
            {
                prev_depth += 1;
            }
            else if (prev->type == JSON_TOKEN_LEFT_BRACKET ||
                     JSON_TOKEN_LEFT_SQUACKET)
            {
                if (prev_depth == 0)
                {
                    // This is the parent
                    found_parent       = true;
                    parent_is_list     = prev->type == JSON_TOKEN_LEFT_SQUACKET;
                    parent_token_index = (state->token_index - 1 - offset);
                    break;
                }
                prev_depth -= 1;
            }
        }

        if (found_parent)
        {
            current_token->parent_token_index = parent_token_index;
        }

        if (tok_type == JSON_TOKEN_STRING || tok_type == JSON_TOKEN_NUMBER)
        {
            current_token->string_len   = state->string_len;
            current_token->string_start = state->string_start;

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

            current_token->data = buff;

            // Go back to current pos
            fseek(state->fp, fpos, SEEK_SET);
        }
    }

    // Check for a new list value so we can update the parser's memory object

    // Note that this happens BEFORE we update the parent property stack to
    // prevent opening and closing brackets for the same value (object value or
    // list value) to be counted as seperate values

    bool has_parent     = false;
    bool parent_is_list = false;
    if (state->parent_type_stack_size > 0)
    {
        JsonParentType parent_type =
            state->parent_type_stack[state->parent_type_stack_size - 1];

        has_parent     = true;
        parent_is_list = parent_type == JSON_PARENT_TYPE_LIST;
    }

    // TODO: This logic could really use a refactor
    if (tok_type != JSON_TOKEN_COMMA && tok_type != JSON_TOKEN_RIGHT_SQUACKET &&
        has_parent && parent_is_list && !info->settings.make_seq)
    {
        // If we're in a list and we parsed a token that ISN'T a comma or
        // closing squacket, this must be a new list value
        info->mem_obj->num_values += 1;
    }

    // NOW we update the parent type stack

    if (tok_type == JSON_TOKEN_RIGHT_BRACKET)
    {
        // TODO: Check this at runtime
        assert(state->parent_type_stack_size > 0);

        state->parent_type_stack_size -= 1;
    }
    else if (tok_type == JSON_TOKEN_RIGHT_SQUACKET)
    {
        // TODO: Check this at runtime
        assert(state->parent_type_stack_size > 0);

        state->parent_type_stack_size -= 1;
    }
    else if (tok_type == JSON_TOKEN_LEFT_BRACKET)
    {
        // TODO: Check this at runtime
        assert(
            state->parent_type_stack_size < state->parent_type_stack_capacity);

        state->parent_type_stack_size += 1;
        state->parent_type_stack[state->parent_type_stack_size - 1] =
            JSON_PARENT_TYPE_OBJECT;
    }
    else if (tok_type == JSON_TOKEN_LEFT_SQUACKET)
    {
        // TODO: Check this at runtime
        assert(
            state->parent_type_stack_size < state->parent_type_stack_capacity);

        state->parent_type_stack_size += 1;
        state->parent_type_stack[state->parent_type_stack_size - 1] =
            JSON_PARENT_TYPE_LIST;
    }

    state->token_index += 1;

    return true;
}

/**
 * @brief Tokenize (or prepare for tokenization) a JSON string
 *
 * @details This function has two modes count and build. The mode is determined
 * by `settings.make_seq.`  Count determines how many tokens this will need,
 * build assumes you know the number of tokens already and tokenizes the
 * contents of the file.
 *
 * This should be able to compute all memory requirements: Number of tokens,
 * number of properties, number of values. On the second pass, it should also be
 * able to compute the number of properties for each object.
 *
 * @param fp File descriptor to a JSON file
 * @param info[in/out] Lexer info object
 * @param err_msg[out] Error message populated on failure
 * @return [TODO:return]
 */
bool json_lex(FILE* fp, JsonLexerInfo* info, char** err_msg)
{
    printf("---------- Json lexer ----------\n");
    printf("\tMake seq: %s\n", info->settings.make_seq ? "True" : "False");
    printf("\tNumber of tokens: %d\n", info->mem_obj->num_tokens);
    printf("\tLog:\n");

    // TODO: Make this a constant
    const size_t default_parent_type_stack_capacity = 32;
    static JsonParentType
        parent_type_stack_mem[default_parent_type_stack_capacity];

    JsonLexerState state = {
        .in_string                       = false,
        .in_number                       = false,
        .parent_type_stack               = parent_type_stack_mem,
        .parent_type_stack_size          = 0,
        .parent_type_stack_capacity      = default_parent_type_stack_capacity,
        .escaped                         = false,
        .string_start                    = 0,
        .string_len                      = 0,
        .depth                           = 0,
        .token_index                     = 0,
        .fp                              = fp,
        .depth                           = 0,
        .max_depth                       = 0,
        .last_object_or_list_token_index = 0,
    };

    if (!info->settings.make_seq)
    {
        // Prepare memory object
        info->mem_obj->num_properties = 0;
        info->mem_obj->num_values     = 0;
    }

    size_t num_toks = 0;

    JsonToken* seq  = NULL;
    if (info->settings.make_seq)
    {
        // Build token sequence
        num_toks = info->mem_obj->num_tokens;
        seq      = info->mem_obj->tokens_mem;
        printf("\t- Allocated memory for %d tokens\n", num_toks);
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
                    if (!add_tok(&state, seq, JSON_TOKEN_STRING, info, err_msg))
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
                if (!state.settings.make_seq)
                {
                    // Colons map 1-to-1 onto properties
                    info->mem_obj->num_properties += 1;
                }

                // =========
                // New token
                // =========
                if (!add_tok(&state, seq, JSON_TOKEN_COLON, info, err_msg))
                {
                    return false;
                }
            }
            else if (c == ',')
            {
                if (state.in_number)
                {
                    state.in_number = false;

                    if (!add_tok(&state, seq, JSON_TOKEN_NUMBER, info, err_msg))
                    {
                        return 0;
                    }
                }

                // =========
                // New token
                // =========
                if (!add_tok(&state, seq, JSON_TOKEN_COMMA, info, err_msg))
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
                if (!add_tok(
                        &state, seq, JSON_TOKEN_LEFT_BRACKET, info, err_msg))
                {
                    return false;
                }
            }
            else if (c == '}')
            {
                if (state.in_number)
                {
                    state.in_number = false;

                    if (!add_tok(&state, seq, JSON_TOKEN_NUMBER, info, err_msg))
                    {
                        return 0;
                    }
                }

                state.depth -= 1;

                // =========
                // New token
                // =========
                if (!add_tok(
                        &state, seq, JSON_TOKEN_RIGHT_BRACKET, info, err_msg))
                {
                    return false;
                }
            }
            else if (c == '[')
            {
                // =========
                // New token
                // =========
                if (!add_tok(
                        &state, seq, JSON_TOKEN_LEFT_SQUACKET, info, err_msg))
                {
                    return false;
                }
            }
            else if (c == ']')
            {
                if (state.in_number)
                {
                    state.in_number = false;

                    if (!add_tok(&state, seq, JSON_TOKEN_NUMBER, info, err_msg))
                    {
                        return 0;
                    }
                }

                // =========
                // New token
                // =========
                if (!add_tok(
                        &state, seq, JSON_TOKEN_RIGHT_SQUACKET, info, err_msg))
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
                    if (!add_tok(&state, seq, JSON_TOKEN_NUMBER, info, err_msg))
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

    if (info->settings.make_seq)
    {
        info->mem_obj->tokens_mem = seq;
    }
    else
    {
        info->mem_obj->num_tokens          = state.token_index;
        info->mem_obj->path_stack_capacity = state.max_depth;

        // TODO: Do we still need this?
        info->max_depth = state.max_depth;
    }

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

/**
 * @brief Parse a JSON string
 *
 * @param escaped_str[in] String we treat as escaped
 * @param str[out] Pointer to output data
 * @param escaped_str_len Length of `escaped_str`
 * @param len Expected length of the output
 * @param err_msg Error message populated on failure
 * @return True on success, false otherwise
 */
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
    PARSE_STATE_INITIAL_STATE,  // Allowed tokens: string (property name), right
                                // bracket
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
    // clang-format off
    switch (state)
    {
        case PARSE_STATE_INITIAL_STATE:                 return "Initial State";
        case PARSE_STATE_OUT_OF_ROOT:                   return "Out of Root";
        case PARSE_STATE_FAIL:                          return "Failure";
        case PARSE_STATE_OBJECT_AFTER_COLON:            return "Object, After Colon";
        case PARSE_STATE_OBJECT_AFTER_COMMA:            return "Object, After Comma";
        case PARSE_STATE_OBJECT_AFTER_LEFT_BRACKET:     return "Object, After Left Bracket";
        case PARSE_STATE_OBJECT_AFTER_PROPERTY_NAME:    return "Object, After Property Name";
        case PARSE_STATE_OBJECT_AFTER_VALUE:            return "Object, After Value";
        case PARSE_STATE_LIST_AFTER_COMMA:              return "List, After Comma";
        case PARSE_STATE_LIST_AFTER_LEFT_BRACKET:       return "List, After Left Bracket";
        case PARSE_STATE_LIST_AFTER_VALUE:              return "List, After Value";
        default:                                        return "Undefined";
    }
    // clang-format on
}

// Point `value` to an object `num_properties` properties, pushing a new frame
// onto the parse stack if necessary (e.g. if the value is an object or a list).
void set_value_to_object(JsonValue* value,
    size_t num_properties,
    JsonCursor* path_stack,
    size_t* path_stack_size,
    JsonParserMemoryObject* mem_obj,
    JsonSettings* settings)
{
    bool computing_mem_reqs = settings->computing_mem_reqs;

    if (!computing_mem_reqs)
    {
        // clang-format off
        *value = (JsonValue){
            .type = JSON_OBJECT,
            .object = (JsonObject){
                .num_properties = num_properties,
                .properties     = mem_obj->properties_mem + mem_obj->properties_offset,
            },
        };
        // clang-format on

        mem_obj->properties_offset += num_properties;
    }

    *path_stack_size += 1;

    JsonCursor new_cursor = {
        .type = JSON_CURSOR_TYPE_OBJECT,
    };

    if (!computing_mem_reqs)
    {
        JsonObject* new_object = &value->object;

        printf("- New object with %d properties\n", new_object->num_properties);

        new_cursor.object = new_object;
    }

    path_stack[*path_stack_size - 1] = new_cursor;
}

// Point `value` to a list with `num_values` values, pushing a new frame onto
// the parse stack if necessary (e.g. if the value is an object or a list).
void set_value_to_list(JsonValue* value,
    size_t num_values,
    JsonCursor* path_stack,
    size_t* path_stack_size,
    JsonParserMemoryObject* mem_obj,
    JsonSettings* settings)
{
    bool computing_mem_reqs = settings->computing_mem_reqs;

    if (!computing_mem_reqs)
    {
        // clang-format off
        *value = (JsonValue){
            .type = JSON_LIST,
            .list = (JsonList){
                .num_values = num_values,
                .values = mem_obj->values_mem + mem_obj->values_offset,
            },
        };
        // clang-format on

        mem_obj->values_offset += num_values;
    }

    *path_stack_size += 1;

    JsonCursor new_cursor = {
        .type = JSON_CURSOR_TYPE_LIST,
    };

    if (!computing_mem_reqs)
    {
        JsonList* new_list = &value->list;

        printf("- New list with %d values\n", new_list->num_values);

        new_cursor.list = new_list;
    }

    path_stack[*path_stack_size - 1] = new_cursor;
}

// Assign a value to the last property of the object held by the current cursor,
// pushing a new frame onto the parse stack if necessary (e.g. if the value is
// an object or a list).
JsonState set_value_for_json_property(JsonCursor* cursor,
    JsonToken token,
    JsonCursor* path_stack,
    size_t* path_stack_size,
    JsonParserMemoryObject* mem_obj,
    JsonSettings* settings)
{
    bool computing_mem_reqs        = settings->computing_mem_reqs;

    JsonObject* current_object     = NULL;
    JsonProperty* properties       = NULL;
    size_t num_properties          = 0;
    JsonProperty* last_property    = NULL;
    JsonValue* last_property_value = NULL;

    if (!computing_mem_reqs)
    {
        current_object = cursor->object;

        assert(current_object != NULL);

        properties     = current_object->properties;
        num_properties = current_object->num_properties;

        assert(properties != NULL);
        assert(num_properties > 0);
        assert(cursor->index < num_properties);

        last_property = properties + cursor->index;

        assert(last_property != NULL);  // TODO: useless

        last_property_value = &last_property->value;
    }

    if (token.type == JSON_TOKEN_STRING)
    {
        if (!computing_mem_reqs)
        {
            *last_property_value = (JsonValue){
                .type   = JSON_STRING,
                .string = token.string,
            };
        }

        return PARSE_STATE_OBJECT_AFTER_VALUE;
    }
    else if (token.type == JSON_TOKEN_NUMBER)
    {
        if (!computing_mem_reqs)
        {
            *last_property_value = (JsonValue){
                .type   = JSON_NUMBER,
                .number = token.number,

            };
        }

        return PARSE_STATE_OBJECT_AFTER_VALUE;
    }
    else if (token.type == JSON_TOKEN_LEFT_BRACKET)
    {
        set_value_to_object(last_property_value, token.num_properties,
            path_stack, path_stack_size, mem_obj, settings);

        return PARSE_STATE_OBJECT_AFTER_LEFT_BRACKET;
    }
    else if (token.type == JSON_TOKEN_LEFT_SQUACKET)
    {
        set_value_to_list(last_property_value, token.num_values, path_stack,
            path_stack_size, mem_obj, settings);

        return PARSE_STATE_LIST_AFTER_LEFT_BRACKET;
    }
    else
    {
        return PARSE_STATE_FAIL;
    }
}

// Add a value to the list held by the current cursor, pushing a new frame onto
// the parse stack if necessary (e.g. if the value is an object or a list).
JsonState append_value_to_json_list(JsonCursor* cursor,
    JsonToken token,
    JsonCursor* path_stack,
    size_t* path_stack_size,
    JsonParserMemoryObject* mem_obj,
    JsonSettings* settings)
{
    bool computing_mem_reqs = settings->computing_mem_reqs;

    JsonList* current_list  = NULL;
    JsonValue* values       = NULL;
    size_t num_values       = 0;
    JsonValue* last_value   = NULL;

    if (!computing_mem_reqs)
    {
        current_list = cursor->list;

        assert(current_list != NULL);

        values     = current_list->values;
        num_values = current_list->num_values;

        assert(values != NULL);
        assert(num_values > 0);
        assert(cursor->index < num_values);

        last_value = values + cursor->index;
    }

    if (token.type == JSON_TOKEN_STRING)
    {
        if (!computing_mem_reqs)
        {
            *last_value = (JsonValue){
                .type   = JSON_STRING,
                .string = token.string,
            };
        }

        return PARSE_STATE_LIST_AFTER_VALUE;
    }
    else if (token.type == JSON_TOKEN_NUMBER)
    {
        if (!computing_mem_reqs)
        {
            *last_value = (JsonValue){
                .type   = JSON_NUMBER,
                .number = token.number,
            };
        }

        return PARSE_STATE_LIST_AFTER_VALUE;
    }
    else if (token.type == JSON_TOKEN_LEFT_BRACKET)
    {
        set_value_to_object(last_value, token.num_properties, path_stack,
            path_stack_size, mem_obj, settings);

        return PARSE_STATE_OBJECT_AFTER_LEFT_BRACKET;
    }
    else if (token.type == JSON_TOKEN_LEFT_SQUACKET)
    {
        set_value_to_list(last_value, token.num_values, path_stack,
            path_stack_size, mem_obj, settings);

        return PARSE_STATE_LIST_AFTER_LEFT_BRACKET;
    }
    else
    {
        return PARSE_STATE_FAIL;
    }
}

// Pop a frame off of the parse stack and return the next state, which is either
// "after value in list" or "after value in object" depending on the cursor
// type.
JsonState step_down_and_get_next_state(
    JsonCursor* path_stack, size_t* path_stack_size, char** err_msg)
{
    assert(*path_stack_size > 0);

    *path_stack_size -= 1;

    if (*path_stack_size == 0)
    {
        return PARSE_STATE_OUT_OF_ROOT;
    }

    JsonCursor* current_cursor = path_stack + *path_stack_size - 1;

    if (current_cursor->type == JSON_CURSOR_TYPE_OBJECT)
    {
        return PARSE_STATE_OBJECT_AFTER_VALUE;
    }
    else if (current_cursor->type == JSON_CURSOR_TYPE_LIST)
    {
        return PARSE_STATE_LIST_AFTER_VALUE;
    }
    else
    {
        *err_msg = "(parser) internal - unknown cursor type";
        return PARSE_STATE_FAIL;
    }
}

bool parse_json_seq(JsonToken* seq,
    size_t num_tokens,
    JsonCursor* path_stack,
    size_t path_stack_capacity,
    JsonSettings* settings,
    JsonParserMemoryObject* mem_obj,
    JsonValue** root_value_ptr,
    char** err_msg)
{
    size_t path_stack_size = 0;

    JsonValue* root_value  = NULL;

    if (!settings->computing_mem_reqs)
    {
        // Point root value to first value in values memory segment
        root_value      = mem_obj->values_mem + mem_obj->values_offset;
        *root_value_ptr = root_value;

        mem_obj->values_offset += 1;
    }
    else
    {
        mem_obj->num_properties = 0;
        mem_obj->num_values     = 1;  // For the root
    }

    JsonState state = PARSE_STATE_INITIAL_STATE;

    for (size_t i = 0; i < num_tokens; i++)
    {
        JsonToken tok = seq[i];

        printf("\t%d: %s | %s\n", i, parse_state_to_string(state),
            json_tok_type_to_sting(tok));

        JsonTokenType type         = tok.type;
        JsonCursor* current_cursor = NULL;

        if (path_stack_size > 0)
        {
            current_cursor = &path_stack[path_stack_size - 1];
        }

        /* State transition */
        if (state == PARSE_STATE_INITIAL_STATE)
        {
            state = PARSE_STATE_FAIL;

            if (type == JSON_TOKEN_STRING)
            {
                if (!settings->computing_mem_reqs)
                {
                    *root_value = (JsonValue){
                        .type   = JSON_STRING,
                        .string = tok.string,
                    };
                }

                state = PARSE_STATE_OUT_OF_ROOT;
            }
            else if (type == JSON_TOKEN_NUMBER)
            {
                if (!settings->computing_mem_reqs)
                {
                    *root_value = (JsonValue){
                        .type   = JSON_NUMBER,
                        .number = tok.number,
                    };
                }

                state = PARSE_STATE_OUT_OF_ROOT;
            }
            else if (type == JSON_TOKEN_LEFT_BRACKET)
            {
                // ========== New object ==========
                set_value_to_object(root_value, tok.num_properties, path_stack,
                    &path_stack_size, mem_obj, settings);

                state = PARSE_STATE_OBJECT_AFTER_LEFT_BRACKET;
            }
            else if (type == JSON_TOKEN_LEFT_SQUACKET)
            {
                // ========== New list ==========
                set_value_to_list(root_value, tok.num_values, path_stack,
                    &path_stack_size, mem_obj, settings);

                state = PARSE_STATE_LIST_AFTER_LEFT_BRACKET;
            }
            else
            {
                *err_msg =
                    "(parser) expected first token to be a value or an opening "
                    "bracket '[', '{'";
            }
        }
        /* (Object) After left bracket */
        else if (state == PARSE_STATE_OBJECT_AFTER_LEFT_BRACKET)
        {
            state = PARSE_STATE_FAIL;

            assert(current_cursor->type == JSON_CURSOR_TYPE_OBJECT);

            if (type == JSON_TOKEN_STRING)
            {
                // ========== New property ==========
                if (settings->computing_mem_reqs)
                {
                    mem_obj->num_properties += 1;
                }
                else
                {
                    JsonObject* current_object = current_cursor->object;

                    assert(current_object != NULL);
                    assert(
                        current_cursor->index < current_object->num_properties);

                    JsonProperty* properties = current_object->properties;
                    size_t num_properties    = current_object->num_properties;

                    assert(properties != NULL);
                    assert(current_cursor->index < num_properties);

                    JsonProperty* last_property =
                        properties + current_cursor->index;

                    last_property->name = tok.string;
                }

                state = PARSE_STATE_OBJECT_AFTER_PROPERTY_NAME;
            }
            else if (type == JSON_TOKEN_RIGHT_BRACKET)
            {
                state = step_down_and_get_next_state(
                    path_stack, &path_stack_size, err_msg);
            }
            else
            {
                *err_msg =
                    "(parser) in object - expected property name after left "
                    "bracket";
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
                *err_msg =
                    "(parser) in object - expected colon after property name";
            }
        }
        else if (state == PARSE_STATE_OBJECT_AFTER_COLON)
        {
            state = PARSE_STATE_FAIL;

            assert(current_cursor->type == JSON_CURSOR_TYPE_OBJECT);

            if (type == JSON_TOKEN_STRING || type == JSON_TOKEN_NUMBER ||
                type == JSON_TOKEN_LEFT_BRACKET ||
                type == JSON_TOKEN_LEFT_SQUACKET)
            {
                state = set_value_for_json_property(current_cursor, tok,
                    path_stack, &path_stack_size, mem_obj, settings);
            }
            else
            {
                *err_msg = "(parser) in object - expected value after colon";
            }
        }
        /* (Object) After comma */
        else if (state == PARSE_STATE_OBJECT_AFTER_COMMA)
        {
            state = PARSE_STATE_FAIL;

            assert(current_cursor->type == JSON_CURSOR_TYPE_OBJECT);

            if (type == JSON_TOKEN_STRING)
            {
                // ========== New property ==========
                if (settings->computing_mem_reqs)
                {
                    mem_obj->num_properties += 1;
                }
                else
                {
                    JsonObject* current_object = current_cursor->object;

                    assert(current_object != NULL);
                    assert(
                        current_cursor->index < current_object->num_properties);

                    JsonProperty* properties = current_object->properties;
                    size_t num_properties    = current_object->num_properties;

                    assert(properties != NULL);
                    assert(current_cursor->index < num_properties);

                    JsonProperty* last_property =
                        properties + current_cursor->index;

                    last_property->name = tok.string;
                }

                state = PARSE_STATE_OBJECT_AFTER_PROPERTY_NAME;
            }
            else
            {
                *err_msg =
                    "(parser) in object - expected property name after comma";
            }
        }
        else if (state == PARSE_STATE_OBJECT_AFTER_VALUE)
        {
            state = PARSE_STATE_FAIL;

            if (type == JSON_TOKEN_COMMA)
            {
                // We expect another property
                if (!settings->computing_mem_reqs)
                {
                    current_cursor->index += 1;
                }

                state = PARSE_STATE_OBJECT_AFTER_COMMA;
            }
            else if (type == JSON_TOKEN_RIGHT_BRACKET)
            {
                state = step_down_and_get_next_state(
                    path_stack, &path_stack_size, err_msg);
            }
            else
            {
                *err_msg =
                    "(parser) in object - expected comma or right bracket "
                    "after value";
                printf("Bad token type: %s\n", json_tok_type_to_sting(tok));
            }
        }
        /* (List) After comma */
        else if (state == PARSE_STATE_LIST_AFTER_COMMA)
        {
            state = PARSE_STATE_FAIL;

            assert(current_cursor->type == JSON_CURSOR_TYPE_LIST);

            if (type == JSON_TOKEN_STRING || type == JSON_TOKEN_NUMBER ||
                type == JSON_TOKEN_LEFT_BRACKET ||
                type == JSON_TOKEN_LEFT_SQUACKET)
            {
                // ========== New list value ==========
                if (settings->computing_mem_reqs)
                {
                    mem_obj->num_values += 1;
                }

                state = append_value_to_json_list(current_cursor, tok,
                    path_stack, &path_stack_size, mem_obj, settings);
            }
            else
            {
                *err_msg = "(parser) in list - expected value after comma";
            }
        }
        else if (state == PARSE_STATE_LIST_AFTER_LEFT_BRACKET)
        {
            state = PARSE_STATE_FAIL;

            assert(current_cursor->type == JSON_CURSOR_TYPE_LIST);

            if (type == JSON_TOKEN_STRING || type == JSON_TOKEN_NUMBER ||
                type == JSON_TOKEN_LEFT_BRACKET ||
                type == JSON_TOKEN_LEFT_SQUACKET)
            {
                // ========== New list value ==========
                if (settings->computing_mem_reqs)
                {
                    mem_obj->num_values += 1;
                }

                state = append_value_to_json_list(current_cursor, tok,
                    path_stack, &path_stack_size, mem_obj, settings);
            }
            else if (type == JSON_TOKEN_RIGHT_SQUACKET)
            {
                state = step_down_and_get_next_state(
                    path_stack, &path_stack_size, err_msg);
            }
            else
            {
                *err_msg =
                    "(parser) in list - expected value after left squacket";
            }
        }
        else if (state == PARSE_STATE_LIST_AFTER_VALUE)
        {
            state = PARSE_STATE_FAIL;

            if (type == JSON_TOKEN_COMMA)
            {
                // We expect another value
                if (!settings->computing_mem_reqs)
                {
                    current_cursor->index += 1;
                }

                state = PARSE_STATE_LIST_AFTER_COMMA;
            }
            else if (type == JSON_TOKEN_RIGHT_SQUACKET)
            {
                state = step_down_and_get_next_state(
                    path_stack, &path_stack_size, err_msg);
            }
            else if (PARSE_STATE_OUT_OF_ROOT)
            {
                *err_msg =
                    "(parser) in list - out of tokens before stepping out of "
                    "root object";
            }
            else
            {
                *err_msg =
                    "(parser) in list - expected comma or right squacket after "
                    "value";
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
            *err_msg = "(parser) lost - unknown parse state";

            state    = PARSE_STATE_FAIL;
        }
    }

    // Should be out of root now
    if (state != PARSE_STATE_OUT_OF_ROOT)
    {
        // TODO: Amend this error message
        *err_msg =
            "(parser) internal - finished parsing before root object closed";

        // TODO: Clean up memory
        return false;
    }

    // TODO: Clean up memory
    // TODO: Set root object/list
    return true;
}

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
bool parse_json(
    FILE* fp, JsonSettings settings, JsonValue** root_value_ptr, char** err_msg)
{
    JsonParserMemoryObject mem_obj;

    JsonLexerSettings lexer_settings = {
        .make_seq = false,
    };

    JsonLexerInfo lexer_info = {
        .settings = lexer_settings,
        .mem_obj  = &mem_obj,
    };

    // LEXER: First pass

    if (!json_lex(fp, &lexer_info, err_msg))
    {
        return false;
    }

    // Allocate memory for everything all at once
    size_t properties_bytes = mem_obj.num_properties * sizeof(JsonProperty);
    size_t values_bytes     = mem_obj.num_values * sizeof(JsonValue);
    size_t tokens_bytes     = mem_obj.num_tokens * sizeof(JsonToken);
    size_t path_stack_bytes = lexer_info.max_depth * sizeof(JsonCursor);

    size_t json_parse_mem_budget =
        properties_bytes + values_bytes + tokens_bytes + path_stack_bytes;

    uint8_t* mem = (uint8_t*)malloc(json_parse_mem_budget * sizeof(uint8_t));

    mem_obj.properties_mem = (JsonProperty*)mem;
    mem_obj.values_mem     = (JsonValue*)(mem + properties_bytes);
    mem_obj.tokens_mem = (JsonToken*)(mem + properties_bytes + values_bytes);
    mem_obj.path_stack_mem =
        (JsonCursor*)(mem + properties_bytes + values_bytes + tokens_bytes);

    //* Print memory reqs for debugging purposes */
    printf("========== Computed memory requirements ==========\n");
    printf("\tProperties:  %4u, memory: %4u\n", mem_obj.num_properties,
        mem_obj.num_properties * sizeof(JsonProperty));
    printf("\tList values: %4d, memory: %4d\n", mem_obj.num_values,
        mem_obj.num_values * sizeof(JsonValue));
    //* Print memory reqs for debugging purposes */

    // LEXER: Second pass

    lexer_info.settings.make_seq = true;

    rewind(fp);

    if (!json_lex(fp, &lexer_info, err_msg))
    {
        return false;
    }

    // Unpack lexer info
    JsonToken* seq = lexer_info.mem_obj->tokens_mem;

    if (seq == NULL)
    {
        *err_msg = "(parser) oom - couldn't allocate memory for token sequence";
        return false;
    }

    // DEBUG
    printf("========== Token sequence ==========\n");
    for (size_t i = 0; i < mem_obj.num_tokens; i++)
    {
        printf("\t%d: %s\n", i, json_tok_to_sting(seq[i]));
    }

    // PARSER: First pass
    // - Compute the memory we'll need for the output object

    settings.computing_mem_reqs = true;

    printf("========== First parser pass: Compute memory reqs ==========\n");
    if (!parse_json_seq(seq, mem_obj.num_tokens, mem_obj.path_stack_mem,
            mem_obj.path_stack_capacity, &settings, &mem_obj, root_value_ptr,
            err_msg))
    {
        return false;
    }

    //* Print memory reqs for debugging purposes */
    printf("========== Computed memory requirements ==========\n");
    printf("\tProperties:  %4u, memory: %4u\n", mem_obj.num_properties,
        mem_obj.num_properties * sizeof(JsonProperty));
    printf("\tList values: %4d, memory: %4d\n", mem_obj.num_values,
        mem_obj.num_values * sizeof(JsonValue));
    //* Print memory reqs for debugging purposes */

    // PARSER: Second pass
    // - Now we're ready to construct the output object

    settings.computing_mem_reqs = false;

    printf("========== Final parser pass: Build object ==========\n");
    if (!parse_json_seq(seq, mem_obj.num_tokens, mem_obj.path_stack_mem,
            mem_obj.path_stack_capacity, &settings, &mem_obj, root_value_ptr,
            err_msg))
    {
        return false;
    }

    // TODO: Clean up any memory we used that isn't part of the output object

    return true;
}