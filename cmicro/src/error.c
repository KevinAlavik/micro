/*
 * cmicro - Micro compiler
 * Copyright (C) 2025 Kevin Alavik <kevin@alavik.se>
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <error.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define COLOR_RED "\x1b[31m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_BLUE "\x1b[34m"
#define COLOR_RESET "\x1b[0m"

static const char* get_source_line(const char* src, uint32_t line)
{
    if (!src)
        return NULL;
    const char* p       = src;
    uint32_t    current = 1;
    const char* start   = p;

    while (*p && current < line)
    {
        if (*p == '\n')
            current++, start = p + 1;
        p++;
    }

    const char* end = start;
    while (*end && *end != '\n')
        end++;

    static char buf[512];
    size_t      len = end - start;
    if (len >= sizeof(buf))
        len = sizeof(buf) - 1;
    strncpy(buf, start, len);
    buf[len] = '\0';
    return buf;
}

void report_error(const error_t* err)
{
    const char* color = COLOR_RED;
    const char* label = "Error";

    switch (err->level)
    {
    case ERROR_FATAL:
        color = COLOR_RED;
        label = "Error";
        break;
    case ERROR_WARNING:
        color = COLOR_YELLOW;
        label = "Warning";
        break;
    case ERROR_INFO:
        color = COLOR_BLUE;
        label = "Info";
        break;
    default:
        color = COLOR_BLUE;
        label = "Unknown";
        break;
    }

    const char* line_text = get_source_line(err->source, err->line);

    fprintf(stderr, "%s%s%s: %s ", color, label, COLOR_RESET, err->message);

    if (line_text && line_text[0] != '\0')
    {
        fprintf(stderr, "at line %u, column %u\n", err->line, err->column);
        fprintf(stderr, "%s\n", line_text);
        for (uint32_t i = 1; i < err->column; i++)
            fputc(' ', stderr);
        fprintf(stderr, "%s^\n", color);
        fprintf(stderr, COLOR_RESET);
    }
    else
    {
        printf("\n");
    }
}