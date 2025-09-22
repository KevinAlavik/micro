/*
 * cmicro - Micro compiler
 * Copyright (C) 2025 Kevin Alavik <kevin@alavik.se>
 *
 * Licensed under the Apache License, Version 2.0
 */

#ifndef _CMICRO_ERROR_H
#define _CMICRO_ERROR_H

#include <stdint.h>

typedef enum
{
    ERROR_FATAL,
    ERROR_WARNING,
    ERROR_INFO
} error_level_t;

typedef struct
{
    const char*   source;  // full source code
    const char*   message; // error message
    uint32_t      line;    // 1-based line number
    uint32_t      column;  // 1-based column number
    error_level_t level;   // severity
} error_t;

void error_init(const char* source);
void report_error(const error_t* err);

#define ERROR_FATAL(src, line, col, msg) report_error(&(error_t){src, msg, line, col, ERROR_FATAL})
#define ERROR_WARN(src, line, col, msg) report_error(&(error_t){src, msg, line, col, ERROR_WARNING})
#define ERROR_INFO(src, line, col, msg) report_error(&(error_t){src, msg, line, col, ERROR_INFO})

#endif // _CMICRO_ERROR_H