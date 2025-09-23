/*
 * cmicro - Micro compiler
 * Copyright (C) 2025 Kevin Alavik <kevin@alavik.se>
 *
 * Licensed under the Apache License, Version 2.0
 */

#ifndef _CMICRO_LEXER_H
#define _CMICRO_LEXER_H

#include <stdint.h>
#include <stddef.h>

/* ================== */
/* Token type's       */
/* ================== */
typedef enum token_type
{
    /* Literals */
    TOKEN_NLIT,
    TOKEN_FLIT,
    TOKEN_CLIT,
    TOKEN_SLIT,
    TOKEN_BLIT,
    TOKEN_IDENT,
    TOKEN_KEYWORD,

    /* Operators */
    TOKEN_PLUS,    // +
    TOKEN_MINUS,   // -
    TOKEN_STAR,    // *
    TOKEN_SLASH,   // /
    TOKEN_PERCENT, // %
    TOKEN_ASSIGN,  // =
    TOKEN_EQ,      // ==
    TOKEN_NEQ,     // !=
    TOKEN_LT,      // <
    TOKEN_GT,      // >
    TOKEN_LTE,     // <=
    TOKEN_GTE,     // >=

    /* Symbols / punctuation */
    TOKEN_LPAREN, // (
    TOKEN_RPAREN, // )
    TOKEN_LBRACE, // {
    TOKEN_RBRACE, // }
    TOKEN_SEMI,   // ;
    TOKEN_COMMA,  // ,
    TOKEN_DOT,    // .

    /* Specials */
    TOKEN_ERROR = 0xdeadbeef,
    TOKEN_EOF   = 0xFF
} token_type_t;

typedef struct token
{
    token_type_t type;
    uint32_t     pos;    // absolute byte offset
    uint32_t     line;   // line number
    uint32_t     column; // column number
    const char*  lexeme; // pointer into source buffer
    size_t       len;    // length of lexeme

    union
    {
        int64_t i64; // integer literals
        double  f64; // float literals
        char    ch;  // char literals
        struct
        {
            const char* x; // pointer
            int64_t     y; // lenght
        } str;             // string literals
    } value;
} token_t;

/* ============================== */
/* Token -> string                */
/* ============================== */
#define TOKEN_TYPE_STR(t)                                                                          \
    ((t) == TOKEN_NLIT      ? "NLIT"                                                               \
     : (t) == TOKEN_FLIT    ? "FLIT"                                                               \
     : (t) == TOKEN_CLIT    ? "CLIT"                                                               \
     : (t) == TOKEN_SLIT    ? "SLIT"                                                               \
     : (t) == TOKEN_BLIT    ? "BLIT"                                                               \
     : (t) == TOKEN_IDENT   ? "IDENT"                                                              \
     : (t) == TOKEN_KEYWORD ? "KEYWORD"                                                            \
     : (t) == TOKEN_PLUS    ? "PLUS"                                                               \
     : (t) == TOKEN_MINUS   ? "MINUS"                                                              \
     : (t) == TOKEN_STAR    ? "STAR"                                                               \
     : (t) == TOKEN_SLASH   ? "SLASH"                                                              \
     : (t) == TOKEN_PERCENT ? "PERCENT"                                                            \
     : (t) == TOKEN_ASSIGN  ? "ASSIGN"                                                             \
     : (t) == TOKEN_EQ      ? "EQ"                                                                 \
     : (t) == TOKEN_NEQ     ? "NEQ"                                                                \
     : (t) == TOKEN_LT      ? "LT"                                                                 \
     : (t) == TOKEN_GT      ? "GT"                                                                 \
     : (t) == TOKEN_LTE     ? "LTE"                                                                \
     : (t) == TOKEN_GTE     ? "GTE"                                                                \
     : (t) == TOKEN_LPAREN  ? "LPAREN"                                                             \
     : (t) == TOKEN_RPAREN  ? "RPAREN"                                                             \
     : (t) == TOKEN_LBRACE  ? "LBRACE"                                                             \
     : (t) == TOKEN_RBRACE  ? "RBRACE"                                                             \
     : (t) == TOKEN_SEMI    ? "SEMI"                                                               \
     : (t) == TOKEN_COMMA   ? "COMMA"                                                              \
     : (t) == TOKEN_EOF     ? "EOF"                                                                \
                            : "UNKNOWN")

/* ================== */
/* Lexer              */
/* ================== */
typedef struct lexer
{
    const char* src;    // source code buffer
    size_t      len;    // length of source
    size_t      pos;    // current byte offset
    uint32_t    line;   // current line (for error reporting)
    uint32_t    column; // current column
} lexer_t;

token_t lexer_next(lexer_t* lexer);

#endif // _CMICRO_LEXER_H