/*
 * cmicro - Micro compiler
 * Copyright (C) 2025 Kevin Alavik <kevin@alavik.se>
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lexer.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <source_file>\n", argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    FILE*       f        = fopen(filename, "rb");
    if (!f)
    {
        perror("Failed to open file");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);

    char* source = malloc(file_size + 1);
    if (!source)
    {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(f);
        return 1;
    }

    size_t read_bytes = fread(source, 1, file_size, f);
    fclose(f);
    source[read_bytes] = '\0';

    lexer_t lex = {source, read_bytes, 0, 1, 1};

    size_t   capacity = 64;
    size_t   count    = 0;
    token_t* tokens   = malloc(capacity * sizeof(token_t));
    if (!tokens)
    {
        fprintf(stderr, "Memory allocation failed\n");
        free(source);
        return 1;
    }

    while (1)
    {
        token_t tok = lexer_next(&lex);

        if (tok.type == TOKEN_ERROR)
        {
            fprintf(stderr, "Lexing error at [%u:%u]\n", tok.line, tok.column);
            break;
        }

        if (count >= capacity)
        {
            capacity *= 2;
            tokens = realloc(tokens, capacity * sizeof(token_t));
            if (!tokens)
            {
                fprintf(stderr, "Memory allocation failed\n");
                free(source);
                return 1;
            }
        }

        tokens[count++] = tok;

        if (tok.type == TOKEN_EOF)
            break;
    }

    for (size_t i = 0; i < count; i++)
    {
        token_t tok = tokens[i];
        printf("[%4u:%-3u] %-10s %-15.*s", tok.line, tok.column, TOKEN_TYPE_STR(tok.type),
               (int) tok.len, tok.lexeme);

        switch (tok.type)
        {
        case TOKEN_NLIT:
            printf("  (int: %lld)", (long long) tok.value.i64);
            break;
        case TOKEN_FLIT:
            printf("  (float: %f)", tok.value.f64);
            break;
        case TOKEN_CLIT:
            printf("  (char: '%c')", tok.value.ch);
            break;
        case TOKEN_SLIT:
            printf("  (string: \"%.*s\")", (int) tok.value.str.y, tok.value.str.x);
            break;
        case TOKEN_BLIT:
            printf("  (bool: %s)", tok.value.i64 ? "true" : "false");
            break;
        default:
            break;
        }

        printf("\n");
    }

    free(tokens);
    free(source);
    return 0;
}
