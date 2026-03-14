#ifndef __JSON_JL_H__
#define __JSON_JL_H__

#include <stdbool.h>
#include <stdio.h>

typedef enum JsonValueType JsonValueType;
typedef struct JsonObject JsonObject;
typedef struct JsonString JsonString;
typedef struct JsonNumber JsonNumber;
typedef struct JsonList JsonList;
typedef struct JsonProperty JsonProperty;
typedef struct JsonValue JsonValue;

// A JsonObject has a list of JsonProperties. A JsonProperty has a name, and a
// JsonValue. A JsonValue can be a JsonNumber, JsonString,
// JsonObject or a JsonList.

typedef enum JsonValueType
{
    JSON_NUMBER,
    JSON_STRING,
    JSON_LIST,
    JSON_OBJECT
} JsonValueType;

// Primitive implementation. In the future we could use variable precision.
typedef struct JsonNumber
{
    bool is_integer;
    union
    {
        long integer;
        double fraction;
    };
} JsonNumber;

typedef struct JsonString
{
    size_t len;
    bool is_wchar_format;
    union
    {
        char* data;
        char* wchar_data;
    };
} JsonString;

typedef struct JsonList
{
    size_t num_values;
    JsonValue* values;
} JsonList;

typedef struct JsonObject
{
    size_t num_properties;
    JsonProperty* properties;
} JsonObject;

typedef struct JsonValue
{
    JsonValueType type;
    union
    {
        JsonString string;
        JsonNumber number;
        JsonObject object;
        JsonList list;
    };
} JsonValue;

typedef struct JsonProperty
{
    JsonString name;
    JsonValue value;
} JsonProperty;

typedef enum JsonCursorType
{
    JSON_CURSOR_TYPE_LIST,
    JSON_CURSOR_TYPE_OBJECT,
} JsonCursorType;

typedef struct JsonCursor
{
    JsonCursorType type;
    union
    {
        JsonObject* object;
        JsonList* list;
    };
} JsonCursor;

typedef struct JsonSettings
{
} JsonSettings;

bool parse_json_number(
    const char* num_str, size_t len, JsonNumber* number, char** err_msg);

bool parse_json_string(const char* escaped_str,
    char* str,
    size_t escaped_str_len,
    size_t len,
    char** err_msg);

bool parse_json(FILE* fp,
    JsonSettings settings,
    JsonObject** root_object_ptr,
    JsonList** root_list_ptr,
    char** err_msg);

#endif