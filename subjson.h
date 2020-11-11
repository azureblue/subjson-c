#ifndef SUBJSON_H
#define SUBJSON_H

struct subjson_result {
    int start;
    int len;
};

typedef struct subjson_result subjson_result_t;

subjson_result_t subjson(const char* json, const char* path);

#endif