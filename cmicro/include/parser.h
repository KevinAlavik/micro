/*
 * cmicro - Micro compiler
 * Copyright (C) 2025 Kevin Alavik <kevin@alavik.se>
 *
 * Licensed under the Apache License, Version 2.0
 */

#ifndef _CMICRO_PARSER_H
#define _CMICRO_PARSER_H

#include <lexer.h>
#include <stdbool.h>

typedef enum
{
    NODE_BINOP,
    NODE_NUMBER,
    NODE_STRING,
    NODE_RETURN,
    NODE_FUNC_DEF,
    NODE_FUNC_CALL,
    NODE_BLOCK,
    NODE_IDENT,
    NODE_ASSIGN,
    NODE_PROGRAM,
    NODE_IF,
    NODE_ELSEIF,
    NODE_ELSE,
    NODE_IMPORT,
} ast_node_type_t;

typedef struct param_node
{
    char*              name;
    size_t             name_len;
    char*              type;
    struct param_node* next;
    bool               is_variadic;
} param_node_t;

typedef struct
{
    token_type_t     op;
    struct ast_node* left;
    struct ast_node* right;
} ast_binop_t;

typedef struct
{
    union
    {
        int64_t i64;
        double  f64;
    } value;
    token_type_t lit_type;
} ast_number_t;

typedef struct
{
    char*  value;
    size_t len;
} ast_string_t;

typedef struct
{
    char*  name;
    size_t name_len;
} ast_ident_t;

typedef struct
{
    char*            name;
    size_t           name_len;
    char*            type; // NOTE: NULL for assignment, non-NULL for definition
    struct ast_node* value;
} ast_assign_t;

typedef struct
{
    struct ast_node* expr;
} ast_return_t;

typedef struct
{
    char*            name;
    size_t           name_len;
    char*            return_type;
    param_node_t*    params;
    struct ast_node* root;
    bool             is_declaration;
} ast_func_def_t;

typedef struct
{
    char*            name;
    size_t           name_len;
    struct ast_node* args;
    size_t           arg_count;
} ast_func_call_t;

typedef struct
{
    struct ast_node* stmts;
    size_t           stmt_count;
} ast_block_t;

typedef struct
{
    struct ast_node* func_defs;
    size_t           func_def_count;
} ast_program_t;

typedef struct
{
    struct ast_node* condition;
    struct ast_node* then_block;
    struct ast_node* else_block; // NOTE: Can be NULL, or point to NODE_ELSEIF or NODE_ELSE
} ast_if_t;

typedef struct
{
    struct ast_node* condition;
    struct ast_node* then_block;
    struct ast_node* else_block; // NOTE: Can be NULL, or point to NODE_ELSEIF or NODE_ELSE
} ast_elseif_t;

typedef struct
{
    struct ast_node* block;
} ast_else_t;

typedef struct
{
    char* module;
} ast_import_t;

typedef struct ast_node
{
    ast_node_type_t type;
    union
    {
        ast_binop_t     binop;
        ast_number_t    number;
        ast_string_t    string;
        ast_ident_t     ident;
        ast_assign_t    assign;
        ast_return_t    return_stmt;
        ast_func_def_t  func_def;
        ast_func_call_t func_call;
        ast_block_t     block;
        ast_program_t   program;
        ast_if_t        if_stmt;
        ast_elseif_t    elseif_stmt;
        ast_else_t      else_stmt;
        ast_import_t    import;
    } data;
} ast_node_t;

ast_node_t* ast_gen(token_t* tokens);
void        ast_free(ast_node_t* node);

#endif // _CMICRO_PARSER_H