/*
 * cmicro - Micro compiler
 * Copyright (C) 2025 Kevin Alavik <kevin@alavik.se> <kevin@piraterna.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
    token_t tok;
    do
    {
        tok = lexer_next(&lex);
        printf("[%4u:%-3u] %-8s %-12.*s\n", tok.line, tok.column, TOKEN_TYPE_STR(tok.type),
               (int) tok.len, tok.lexeme);
    } while (tok.type != TOKEN_EOF && tok.type != TOKEN_ERROR);

    free(source);
    return 0;
}
