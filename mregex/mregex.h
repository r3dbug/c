#ifndef MREGEX_H
#define MREGEX_H

#include <sys/types.h>
#include <proto/regex.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MREG_NS 10
#define MREG_MATCH_MODE 0
#define MREG_NO_ERRORS 0
#define MREG_CONSOLE 1 // 0 = off, 1 = on
#define MREG_STRING_LENGTH 100
#define MREG_LOOKBEHIND_ERROR 2
#define MREG_NO_CONVERSION 0
#define MREG_CONVERT_TO_UPPER 1
#define MREG_CONVERT_TO_LOWER 2
#define MREG_ITERATIONS_REPLACE 5
#define MREG_CONTEXT_REPLACEMENT 1
#define MREG_PATTERN_REPLACEMENT 0

struct Library *RegexBase;

// prototypes

// public

signed int mreg_match(char*, char*);
signed int mreg_replace(char*, char*, char*, char*);
int mreg_quote(char*, char*);
void mreg_console_on(void);
void mreg_console_off(void);
int mreg_extract(int, char*, char*);
int mreg_init(void);
int mreg_finish(void);

// internal

int mreg_is_escaped(char*, int);
int mreg_eliminate_non_capturing_groups(char*);
signed int mreg_eliminate_lookbehind(char*, char*);
signed int mreg_eliminate_lookahead(char*, char*);
void mreg_eliminate_lookaround(char*, char*, char*);
int mreg_eliminate_nongreedy(char*);
int mreg_build_replace(regmatch_t*, char*, char*, char*);
regmatch_t* mreg_match_shorter(regex_t, char*, regoff_t, regoff_t);
signed int mreg_check_lookbehind(char*, int, int, char*);
signed int mreg_check_lookahead(char*, int, int, char*);
signed int mreg_check_lookaround(char*, int, int, char*, int, int, char*);
signed int mreg_replace(char*, char*, char*, char*);
signed int mreg_replace_all(char*, char*, char*, char*);
signed int mreg_context_replace(char*, char*, char*, char*);
signed int mreg_context_replace_all(char*, char*, char*, char*);

#include "mregex.c"

#endif
