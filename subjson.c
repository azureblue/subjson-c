#include <setjmp.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

static const char* subjson_json;
int json_len;
int json_pos;
static jmp_buf fail_jmp;

noreturn void panic() {
    longjmp(fail_jmp, 1);
}

inline static int is_number(char c) {
    return c >= '0' && c <= '9';
}

inline static int is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static char next() {
    if (json_pos >= json_len)
        panic();
    return subjson_json[json_pos++];
}

static void skip_whitespaces() {
    while (json_pos < json_len)
        if (!is_whitespace(subjson_json[json_pos++])) {
            json_pos--;
            return;
        }
}

static void expect_next(char ch) {
    skip_whitespaces();
    if (next() != ch)
        panic();
}

static void move_after_closing_quote() {
    while (true)
        if (next() == '"' && subjson_json[json_pos - 2] != '\\')
            return;
}

static bool next_matches(const char* name) {
    int nameLen = strlen(name);
    for (int i = 0; i < nameLen; i++) {
        if (next() != name[i])
            return false;
    }
    return true;
}

static void skip_sub_json() {
    skip_whitespaces();
    char ch = next();
    if (ch == '{' || ch == '[') {
        char closing = (char)(ch + 2);
        int openings = 0;
        while (true) {
            char ch_n = next();
            if (ch_n == '"') {
                move_after_closing_quote();
                continue;
            }
            if (ch_n == closing)
                if (openings == 0)
                    return;
                else
                    openings--;
            else if (ch_n == ch)
                openings++;
        }
    }

    if (ch == '"') {
        move_after_closing_quote();
        return;
    }

    for (; json_pos < json_len; json_pos++) {
        ch = subjson_json[json_pos];
        if (ch >= 'a' && ch <= 'u')
            continue;
        if (ch >= '-' && ch <= '9')
            continue;
        if (ch == 'E')
            continue;
        return;
    }
    panic();
}

static void go_to_array(int idx) {
    expect_next('[');
    for (int i = 0; i < idx; i++) {
        skip_sub_json();
        expect_next(',');
    }
}

static void go_to_field(const char* name) {
    expect_next('{');
    while (true) {
        expect_next('"');
        if (next_matches(name)) {
            if (next() == '"') {
                expect_next(':');
                return;
            }
        }
        move_after_closing_quote();
        expect_next(':');
        skip_sub_json();
        expect_next(',');
    }
}

subjson_result_t get() {
    skip_whitespaces();
    int start = json_pos;
    skip_sub_json();
    return (subjson_result_t){.start = start, .len = json_pos - start};
}

subjson_result_t subjson(const char* json, const char* path) {
    subjson_json = json;
    int path_len = strlen(path);
    char buf[32];
    int path_pos = 0;
    if (!setjmp(fail_jmp)) {
        while (path_pos < path_len) {
            char subPathCh = path[path_pos];
            int start = path_pos;
            path_pos++;
            if (subPathCh == '.')
                continue;
            if (subPathCh == '[') {
                char* closing_array = strchr(path + path_pos, ']');
                if (!closing_array)
                    panic();

                int json_len = (closing_array - path - path_pos);

                memcpy(buf, path + path_pos, json_len);
                buf[json_len] = 0;
                int array_idx = atoi(buf);
                go_to_array(array_idx);
                path_pos = path_pos + json_len + 1;
                continue;
            }
            for (; path_pos < path_len; path_pos++) {
                char ch = path[path_pos];
                if (ch == '[' || ch == '.')
                    break;
            }
            int json_len = path_pos - start;
            memcpy(buf, path + start, json_len);
            buf[json_len] = 0;
            go_to_field(buf);
        }
        return get();
    } else {
        return (subjson_result_t){.start = -1, .len = -1};
    }
}
