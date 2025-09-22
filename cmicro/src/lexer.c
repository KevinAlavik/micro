/*
 * cmicro - Micro compiler
 * Copyright (C) 2025 Kevin Alavik <kevin@alavik.se>
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <lexer.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <error.h>

/* ================== */
/* Helper tables      */
/* ================== */
typedef struct
{
    const char*  word;
    token_type_t type;
} keyword_t;

static const keyword_t keywords[] = {
    {"int", TOKEN_KEYWORD},    {"return", TOKEN_KEYWORD}, {"if", TOKEN_KEYWORD},
    {"else", TOKEN_KEYWORD},   {"while", TOKEN_KEYWORD},  {"for", TOKEN_KEYWORD},
    {"void", TOKEN_KEYWORD},   {"char", TOKEN_KEYWORD},   {"float", TOKEN_KEYWORD},
    {"double", TOKEN_KEYWORD},
};
static const size_t keyword_count = sizeof(keywords) / sizeof(keywords[0]);

typedef struct
{
    const char*  op;
    token_type_t type;
} op_t;

static const op_t operators[] = {
    {"+", TOKEN_PLUS},    {"-", TOKEN_MINUS},  {"*", TOKEN_STAR},   {"/", TOKEN_SLASH},
    {"%", TOKEN_PERCENT}, {"=", TOKEN_ASSIGN}, {"==", TOKEN_EQ},    {"!=", TOKEN_NEQ},
    {"<", TOKEN_LT},      {">", TOKEN_GT},     {"<=", TOKEN_LTE},   {">=", TOKEN_GTE},
    {"(", TOKEN_LPAREN},  {")", TOKEN_RPAREN}, {"{", TOKEN_LBRACE}, {"}", TOKEN_RBRACE},
    {";", TOKEN_SEMI},    {",", TOKEN_COMMA},
};
static const size_t op_count = sizeof(operators) / sizeof(operators[0]);

/* ================== */
/* Lexer utilities    */
/* ================== */
static inline char lexer_peek(lexer_t* lex)
{
    return (lex->pos < lex->len) ? lex->src[lex->pos] : '\0';
}

static inline char lexer_advance(lexer_t* lex)
{
    char c = lexer_peek(lex);
    if (c == '\0')
        return c;
    lex->pos++;
    if (c == '\n')
    {
        lex->line++;
        lex->column = 1;
    }
    else
    {
        lex->column++;
    }
    return c;
}

/* Get current source line for error reporting */
static const char* lexer_current_line(lexer_t* lex)
{
    size_t start = lex->pos;
    while (start > 0 && lex->src[start - 1] != '\n')
        start--;

    size_t end = lex->pos;
    while (end < lex->len && lex->src[end] != '\n')
        end++;

    static char line_buf[512];
    size_t      len = end - start;
    if (len >= sizeof(line_buf))
        len = sizeof(line_buf) - 1;
    strncpy(line_buf, &lex->src[start], len);
    line_buf[len] = '\0';
    return line_buf;
}

/* ================== */
/* Skip whitespace and comments */
/* ================== */
static void lexer_skip_ws_and_comments(lexer_t* lex)
{
    for (;;)
    {
        char c = lexer_peek(lex);
        if (isspace((unsigned char) c))
        {
            lexer_advance(lex);
            continue;
        }

        // Line comment //
        if (c == '/' && lex->pos + 1 < lex->len && lex->src[lex->pos + 1] == '/')
        {
            lexer_advance(lex);
            lexer_advance(lex);
            while (lexer_peek(lex) != '\n' && lexer_peek(lex) != '\0')
                lexer_advance(lex);
            continue;
        }

        // Block comment /* ... */
        if (c == '/' && lex->pos + 1 < lex->len && lex->src[lex->pos + 1] == '*')
        {
            lexer_advance(lex);
            lexer_advance(lex);
            while (lex->pos + 1 < lex->len &&
                   !(lexer_peek(lex) == '*' && lex->src[lex->pos + 1] == '/'))
            {
                lexer_advance(lex);
            }
            if (lexer_peek(lex) == '*' && lex->pos + 1 < lex->len)
            {
                lexer_advance(lex);
                lexer_advance(lex);
            }
            else
            {
                ERROR_FATAL(lex->src, lex->line, lex->column, "Unterminated block comment");
            }
            continue;
        }

        break;
    }
}

/* ================== */
/* Lookup functions   */
/* ================== */
static token_type_t lookup_keyword(const char* lexeme, size_t len)
{
    for (size_t i = 0; i < keyword_count; i++)
    {
        if (strlen(keywords[i].word) == len && strncmp(lexeme, keywords[i].word, len) == 0)
        {
            return keywords[i].type;
        }
    }
    return TOKEN_IDENT;
}

static token_type_t lookup_operator(lexer_t* lex, size_t* out_len)
{
    for (size_t i = 0; i < op_count; i++)
    {
        size_t len = strlen(operators[i].op);
        if (lex->pos + len <= lex->len && strncmp(&lex->src[lex->pos], operators[i].op, len) == 0)
        {
            *out_len = len;
            return operators[i].type;
        }
    }
    return TOKEN_IDENT;
}

/* ================== */
/* Main lexer         */
/* ================== */
token_t lexer_next(lexer_t* lex)
{
    lexer_skip_ws_and_comments(lex);

    token_t tok = {0};
    tok.pos     = (uint32_t) lex->pos;
    tok.line    = lex->line;
    tok.column  = lex->column;

    char c = lexer_peek(lex);
    if (c == '\0')
    {
        tok.type = TOKEN_EOF;
        return tok;
    }

    // Numbers (integers, TODO: float's)
    if (isdigit((unsigned char) c))
    {
        size_t start = lex->pos;
        while (isdigit((unsigned char) lexer_peek(lex)))
            lexer_advance(lex);

        size_t length = lex->pos - start;
        tok.lexeme    = &lex->src[start];
        tok.len       = length;
        tok.type      = TOKEN_NLIT;
        tok.value.i64 = strtoll(tok.lexeme, NULL, 10);
        return tok;
    }

    // Identifiers and keywords
    if (isalpha((unsigned char) c) || c == '_')
    {
        size_t start = lex->pos;
        while (isalnum((unsigned char) lexer_peek(lex)) || lexer_peek(lex) == '_')
            lexer_advance(lex);

        size_t length = lex->pos - start;
        tok.lexeme    = &lex->src[start];
        tok.len       = length;
        tok.type      = lookup_keyword(tok.lexeme, length);
        return tok;
    }

    // Operators
    size_t       op_len  = 0;
    token_type_t op_type = lookup_operator(lex, &op_len);
    if (op_type != TOKEN_IDENT)
    {
        tok.type   = op_type;
        tok.lexeme = &lex->src[lex->pos];
        tok.len    = op_len;
        for (size_t i = 0; i < op_len; i++)
            lexer_advance(lex);
        return tok;
    }

    // Unknown or invalid character
    lexer_advance(lex);
    tok.lexeme = &lex->src[tok.pos];
    tok.len    = 1;
    tok.type   = TOKEN_ERROR;

    ERROR_FATAL(lex->src, tok.line, tok.column, "Unexpected character");
    return tok;
}
