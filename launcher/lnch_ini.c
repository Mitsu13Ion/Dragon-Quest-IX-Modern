/* settings.ini without losing anything: see lnch.h */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lnch.h"
#ifdef _WIN32
#include <windows.h>
#endif

/* the line [p, end) sets `key`: then *val to *vend is its value (spaces trimmed) */
static int line_key(const char* p, const char* end, const char* key, const char** val, const char** vend) {
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    size_t k = strlen(key);
    if ((size_t)(end - p) < k || strncmp(p, key, k)) return 0;
    p += k;
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    if (p >= end || *p != '=') return 0;
    p++;
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    const char* e = end;
    while (e > p && (e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t')) e--;
    *val = p; *vend = e;
    return 1;
}

int ini_get(const char* text, const char* key, char* out, size_t n) {
    for (const char* p = text; p && *p;) {
        const char* end = strchr(p, '\n');
        if (!end) end = p + strlen(p);
        const char *v, *ve;
        if (line_key(p, end, key, &v, &ve)) {
            size_t len = (size_t)(ve - v);
            if (len >= n) len = n - 1;
            memcpy(out, v, len); out[len] = 0;
            return 1;
        }
        p = *end ? end + 1 : end;
    }
    return 0;
}

char* ini_set(const char* text, const char* key, const char* value) {
    size_t tl = strlen(text), extra = strlen(key) + strlen(value) + 8;
    char* out = malloc(tl + extra);
    if (!out) return NULL;
    for (const char* p = text; *p;) {
        const char* end = strchr(p, '\n');
        if (!end) end = p + strlen(p);
        const char *v, *ve;
        if (line_key(p, end, key, &v, &ve)) {         /* only the value changes; the spaces and \r around it stay */
            size_t a = (size_t)(v - text);
            memcpy(out, text, a);
            size_t vl = strlen(value);
            memcpy(out + a, value, vl);
            strcpy(out + a + vl, ve);
            return out;
        }
        p = *end ? end + 1 : end;
    }
    memcpy(out, text, tl);
    size_t o = tl;
    if (tl && text[tl - 1] != '\n') out[o++] = '\n';
    sprintf(out + o, "%s = %s\n", key, value);
    return out;
}

int ini_load(const char* path, char** text) {
    FILE* f = fopen(path, "rb");
    if (!f) { *text = calloc(1, 1); return 0; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0 || n > (1 << 20)) n = 0;                  /* settings.ini is a few KB */
    *text = malloc((size_t)n + 1);
    size_t got = *text ? fread(*text, 1, (size_t)n, f) : 0;
    fclose(f);
    if (!*text) { *text = calloc(1, 1); return 0; }
    (*text)[got] = 0;
    return 1;
}

int ini_save(const char* path, const char* text) {
    char tmp[1100];
    if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp) return 0;
    FILE* f = fopen(tmp, "wb");
    if (!f) return 0;
    size_t n = strlen(text);
    int ok = fwrite(text, 1, n, f) == n;
    ok = fclose(f) == 0 && ok;
    if (!ok) { remove(tmp); return 0; }
#ifdef _WIN32
    return MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING) != 0;   /* rename() doesn't replace there */
#else
    return rename(tmp, path) == 0;
#endif
}
