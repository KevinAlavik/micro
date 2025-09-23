/*
 * cmicro - Micro compiler
 * Copyright (C) 2025 Kevin Alavik <kevin@alavik.se>
 *
 * Licensed under the Apache License, Version 2.0
 */

#define _GNU_SOURCE
#include <parser.h>
#include <stdlib.h>
#include <error.h>
#include <string.h>
#include <stdbool.h>

static bool error = false;

static void ast_free_internal(ast_node_t* node);

/* ================== */
/* Parser utilities   */
/* ================== */
typedef struct parser
{
    token_t* tokens;
    size_t   pos;
} parser_t;

static inline token_t parser_peek(parser_t* parser)
{
    return parser->tokens[parser->pos];
}

static inline token_t parser_advance(parser_t* parser)
{
    return parser->tokens[parser->pos++];
}

static void parser_error(parser_t* parser, const char* message)
{
    if (!error)
    {
        token_t tok = parser_peek(parser);
        ERROR_FATAL("", tok.line, tok.column, message);
        error = true;
    }
}

static int get_precedence(token_type_t op)
{
    switch (op)
    {
    case TOKEN_STAR:
    case TOKEN_SLASH:
    case TOKEN_PERCENT:
        return 3;
    case TOKEN_PLUS:
    case TOKEN_MINUS:
        return 2;
    case TOKEN_EQ:
    case TOKEN_NEQ:
    case TOKEN_LT:
    case TOKEN_GT:
    case TOKEN_LTE:
    case TOKEN_GTE:
        return 1;
    case TOKEN_ASSIGN:
        return 0;
    default:
        return -1;
    }
}

/* ================== */
/* Node utilities     */
/* ================== */
static ast_node_t* ast_create_binop(token_type_t op, ast_node_t* left, ast_node_t* right)
{
    if (error)
        return NULL;
    ast_node_t* node = (ast_node_t*) malloc(sizeof(ast_node_t));
    if (!node)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for binop node");
        error = true;
        return NULL;
    }
    node->type             = NODE_BINOP;
    node->data.binop.op    = op;
    node->data.binop.left  = left;
    node->data.binop.right = right;
    return node;
}

static ast_node_t* ast_create_number_int(int64_t value)
{
    if (error)
        return NULL;
    ast_node_t* node = (ast_node_t*) malloc(sizeof(ast_node_t));
    if (!node)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for number node");
        error = true;
        return NULL;
    }
    node->type                  = NODE_NUMBER;
    node->data.number.value.i64 = value;
    node->data.number.lit_type  = TOKEN_NLIT;
    return node;
}

static ast_node_t* ast_create_number_float(double value)
{
    if (error)
        return NULL;
    ast_node_t* node = (ast_node_t*) malloc(sizeof(ast_node_t));
    if (!node)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for number node");
        error = true;
        return NULL;
    }
    node->type                  = NODE_NUMBER;
    node->data.number.value.f64 = value;
    node->data.number.lit_type  = TOKEN_FLIT;
    return node;
}

static ast_node_t* ast_create_string(char* value, size_t len)
{
    if (error)
        return NULL;
    ast_node_t* node = (ast_node_t*) malloc(sizeof(ast_node_t));
    if (!node)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for string node");
        error = true;
        return NULL;
    }
    node->type              = NODE_STRING;
    node->data.string.value = value;
    node->data.string.len   = len;
    return node;
}

static ast_node_t* ast_create_ident(char* name, size_t name_len)
{
    if (error)
        return NULL;
    ast_node_t* node = (ast_node_t*) malloc(sizeof(ast_node_t));
    if (!node)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for ident node");
        error = true;
        return NULL;
    }
    node->type                = NODE_IDENT;
    node->data.ident.name     = name;
    node->data.ident.name_len = name_len;
    return node;
}

static ast_node_t* ast_create_assign(char* name, size_t name_len, char* type, ast_node_t* value)
{
    if (error)
        return NULL;
    ast_node_t* node = (ast_node_t*) malloc(sizeof(ast_node_t));
    if (!node)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for assign node");
        error = true;
        return NULL;
    }
    node->type                 = NODE_ASSIGN;
    node->data.assign.name     = name;
    node->data.assign.name_len = name_len;
    node->data.assign.type     = type;
    node->data.assign.value    = value;
    return node;
}

static ast_node_t* ast_create_return(ast_node_t* expr)
{
    if (error)
        return NULL;
    ast_node_t* node = (ast_node_t*) malloc(sizeof(ast_node_t));
    if (!node)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for return node");
        error = true;
        return NULL;
    }
    node->type                  = NODE_RETURN;
    node->data.return_stmt.expr = expr;
    return node;
}

static ast_node_t* ast_create_func_def(char* name, size_t name_len, char* return_type,
                                       param_node_t* params, ast_node_t* root, bool is_declaration)
{
    if (error)
        return NULL;
    ast_node_t* node = (ast_node_t*) malloc(sizeof(ast_node_t));
    if (!node)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for func_def node");
        error = true;
        return NULL;
    }
    node->type                         = NODE_FUNC_DEF;
    node->data.func_def.name           = name;
    node->data.func_def.name_len       = name_len;
    node->data.func_def.return_type    = return_type;
    node->data.func_def.params         = params;
    node->data.func_def.root           = root;
    node->data.func_def.is_declaration = is_declaration;
    return node;
}

static ast_node_t* ast_create_func_call(char* name, size_t name_len, ast_node_t* args,
                                        size_t arg_count)
{
    if (error)
        return NULL;
    ast_node_t* node = (ast_node_t*) malloc(sizeof(ast_node_t));
    if (!node)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for func_call node");
        error = true;
        return NULL;
    }
    node->type                     = NODE_FUNC_CALL;
    node->data.func_call.name      = name;
    node->data.func_call.name_len  = name_len;
    node->data.func_call.args      = args;
    node->data.func_call.arg_count = arg_count;
    return node;
}

static ast_node_t* ast_create_block(ast_node_t* stmts, size_t stmt_count)
{
    if (error)
        return NULL;
    ast_node_t* node = (ast_node_t*) malloc(sizeof(ast_node_t));
    if (!node)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for block node");
        error = true;
        return NULL;
    }
    node->type                  = NODE_BLOCK;
    node->data.block.stmts      = stmts;
    node->data.block.stmt_count = stmt_count;
    return node;
}

static ast_node_t* ast_create_program(ast_node_t* func_defs, size_t func_def_count)
{
    if (error)
        return NULL;
    ast_node_t* node = (ast_node_t*) malloc(sizeof(ast_node_t));
    if (!node)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for program node");
        error = true;
        return NULL;
    }
    node->type                        = NODE_PROGRAM;
    node->data.program.func_defs      = func_defs;
    node->data.program.func_def_count = func_def_count;
    return node;
}

static ast_node_t* ast_create_if(ast_node_t* condition, ast_node_t* then_block,
                                 ast_node_t* else_block)
{
    if (error)
        return NULL;
    ast_node_t* node = (ast_node_t*) malloc(sizeof(ast_node_t));
    if (!node)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for if node");
        error = true;
        return NULL;
    }
    node->type                    = NODE_IF;
    node->data.if_stmt.condition  = condition;
    node->data.if_stmt.then_block = then_block;
    node->data.if_stmt.else_block = else_block;
    return node;
}

static ast_node_t* ast_create_elseif(ast_node_t* condition, ast_node_t* then_block,
                                     ast_node_t* else_block)
{
    if (error)
        return NULL;
    ast_node_t* node = (ast_node_t*) malloc(sizeof(ast_node_t));
    if (!node)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for elseif node");
        error = true;
        return NULL;
    }
    node->type                        = NODE_ELSEIF;
    node->data.elseif_stmt.condition  = condition;
    node->data.elseif_stmt.then_block = then_block;
    node->data.elseif_stmt.else_block = else_block;
    return node;
}

static ast_node_t* ast_create_else(ast_node_t* block)
{
    if (error)
        return NULL;
    ast_node_t* node = (ast_node_t*) malloc(sizeof(ast_node_t));
    if (!node)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for else node");
        error = true;
        return NULL;
    }
    node->type                 = NODE_ELSE;
    node->data.else_stmt.block = block;
    return node;
}

static param_node_t* ast_create_param(char* name, size_t name_len, char* type)
{
    if (error)
        return NULL;
    param_node_t* param = (param_node_t*) malloc(sizeof(param_node_t));
    if (!param)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for param node");
        error = true;
        return NULL;
    }
    param->name        = name;
    param->name_len    = name_len;
    param->type        = type;
    param->next        = NULL;
    param->is_variadic = false;
    return param;
}

static ast_node_t* ast_create_import(char* module)
{
    if (error)
        return NULL;
    ast_node_t* node = (ast_node_t*) malloc(sizeof(ast_node_t));
    if (!node)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for import node");
        error = true;
        return NULL;
    }
    node->type               = NODE_IMPORT;
    node->data.import.module = module;
    return node;
}

/* ================== */
/* Parsers            */
/* ================== */
static ast_node_t*   parse_factor(parser_t* parser);
static ast_node_t*   parse_expression(parser_t* parser, int min_precedence);
static ast_node_t*   parse_statement(parser_t* parser);
static param_node_t* parse_param_list(parser_t* parser);
static ast_node_t*   parse_func_def(parser_t* parser);
static ast_node_t*   parse_func_call(parser_t* parser);
static ast_node_t*   parse_if_statement(parser_t* parser);

static ast_node_t* parse_factor(parser_t* parser)
{
    if (error)
        return NULL;
    token_t tok = parser_peek(parser);

    if (tok.type == TOKEN_NLIT)
    {
        parser_advance(parser);
        return ast_create_number_int(tok.value.i64);
    }
    else if (tok.type == TOKEN_FLIT)
    {
        parser_advance(parser);
        return ast_create_number_float(tok.value.f64);
    }
    else if (tok.type == TOKEN_SLIT)
    {
        parser_advance(parser);
        char* str = strndup(tok.value.str.x, tok.value.str.y);
        if (!str)
        {
            ERROR_FATAL("", 0, 0, "Memory allocation failed for string");
            error = true;
            return NULL;
        }
        return ast_create_string(str, tok.value.str.y);
    }
    else if (tok.type == TOKEN_IDENT)
    {
        token_t ident_tok = parser_advance(parser);
        if (parser_peek(parser).type == TOKEN_LPAREN)
        {
            parser->pos--;
            return parse_func_call(parser);
        }
        char* name = strndup(ident_tok.lexeme, ident_tok.len);
        if (!name)
        {
            ERROR_FATAL("", 0, 0, "Memory allocation failed for identifier");
            error = true;
            return NULL;
        }
        return ast_create_ident(name, ident_tok.len);
    }
    else if (tok.type == TOKEN_LPAREN)
    {
        parser_advance(parser);
        ast_node_t* expr = parse_expression(parser, 0);
        if (error)
            return NULL;
        if (parser_peek(parser).type != TOKEN_RPAREN)
        {
            parser_error(parser, "Expected ')'");
            ast_free(expr);
            return NULL;
        }
        parser_advance(parser);
        return expr;
    }

    parser_error(parser, "Expected number, string, identifier, or '('");
    return NULL;
}

static ast_node_t* parse_expression(parser_t* parser, int min_precedence)
{
    if (error)
        return NULL;
    ast_node_t* left = parse_factor(parser);
    if (error)
        return NULL;

    while (1)
    {
        token_t tok        = parser_peek(parser);
        int     precedence = get_precedence(tok.type);
        if (precedence < min_precedence)
        {
            break;
        }

        token_type_t op = tok.type;
        parser_advance(parser);
        ast_node_t* right = parse_expression(parser, precedence + 1);
        if (error)
        {
            ast_free(left);
            return NULL;
        }
        left = ast_create_binop(op, left, right);
        if (error)
        {
            return NULL;
        }
    }

    return left;
}

static param_node_t* parse_param_list(parser_t* parser)
{
    if (error)
        return NULL;
    if (parser_peek(parser).type != TOKEN_LPAREN)
    {
        parser_error(parser, "Expected '(' for parameter list");
        return NULL;
    }
    parser_advance(parser);

    param_node_t* head = NULL;
    param_node_t* tail = NULL;

    while (parser_peek(parser).type != TOKEN_RPAREN)
    {
        if (parser_peek(parser).type == TOKEN_ELLIPSIS)
        {
            parser_advance(parser);
            param_node_t* param = ast_create_param(NULL, 0, NULL);
            if (!param)
            {
                while (head)
                {
                    param_node_t* next = head->next;
                    free(head->name);
                    free(head->type);
                    free(head);
                    head = next;
                }
                return NULL;
            }
            param->is_variadic = true;
            if (!head)
                head = tail = param;
            else
            {
                tail->next = param;
                tail       = param;
            }
            if (parser_peek(parser).type == TOKEN_COMMA)
            {
                parser_error(parser, "Variadic parameter must be the last in the list");
                while (head)
                {
                    param_node_t* next = head->next;
                    free(head->name);
                    free(head->type);
                    free(head);
                    head = next;
                }
                return NULL;
            }
            break;
        }

        token_t type_tok = parser_peek(parser);
        if (type_tok.type != TOKEN_KEYWORD)
        {
            parser_error(parser, "Expected type in parameter list");
            while (head)
            {
                param_node_t* next = head->next;
                free(head->name);
                free(head->type);
                free(head);
                head = next;
            }
            return NULL;
        }
        parser_advance(parser);

        token_t name_tok = parser_peek(parser);
        if (name_tok.type != TOKEN_IDENT)
        {
            parser_error(parser, "Expected identifier in parameter list");
            while (head)
            {
                param_node_t* next = head->next;
                free(head->name);
                free(head->type);
                free(head);
                head = next;
            }
            return NULL;
        }
        parser_advance(parser);

        char* type_name = strndup(type_tok.lexeme, type_tok.len);
        if (!type_name)
        {
            ERROR_FATAL("", 0, 0, "Memory allocation failed for type name");
            error = true;
            while (head)
            {
                param_node_t* next = head->next;
                free(head->name);
                free(head->type);
                free(head);
                head = next;
            }
            return NULL;
        }
        char* param_name = strndup(name_tok.lexeme, name_tok.len);
        if (!param_name)
        {
            free(type_name);
            ERROR_FATAL("", 0, 0, "Memory allocation failed for param name");
            error = true;
            while (head)
            {
                param_node_t* next = head->next;
                free(head->name);
                free(head->type);
                free(head);
                head = next;
            }
            return NULL;
        }
        param_node_t* param = ast_create_param(param_name, name_tok.len, type_name);
        if (!param)
        {
            free(type_name);
            free(param_name);
            while (head)
            {
                param_node_t* next = head->next;
                free(head->name);
                free(head->type);
                free(head);
                head = next;
            }
            return NULL;
        }

        if (!head)
            head = tail = param;
        else
        {
            tail->next = param;
            tail       = param;
        }

        if (parser_peek(parser).type == TOKEN_COMMA)
            parser_advance(parser);
        else if (parser_peek(parser).type != TOKEN_RPAREN)
        {
            parser_error(parser, "Expected ',' or ')' in parameter list");
            while (head)
            {
                param_node_t* next = head->next;
                free(head->name);
                free(head->type);
                free(head);
                head = next;
            }
            return NULL;
        }
    }

    if (parser_peek(parser).type != TOKEN_RPAREN)
    {
        parser_error(parser, "Expected ')' to close parameter list");
        while (head)
        {
            param_node_t* next = head->next;
            free(head->name);
            free(head->type);
            free(head);
            head = next;
        }
        return NULL;
    }
    parser_advance(parser);
    return head;
}

static ast_node_t* parse_func_def(parser_t* parser)
{
    if (error)
        return NULL;
    token_t return_type_tok = parser_peek(parser);
    if (return_type_tok.type != TOKEN_KEYWORD)
    {
        parser_error(parser, "Expected return type for function definition");
        return NULL;
    }
    parser_advance(parser);

    token_t name_tok = parser_peek(parser);
    if (name_tok.type != TOKEN_IDENT)
    {
        parser_error(parser, "Expected function name");
        return NULL;
    }
    parser_advance(parser);

    param_node_t* params = parse_param_list(parser);
    if (error)
    {
        while (params)
        {
            param_node_t* next = params->next;
            free(params->name);
            free(params->type);
            free(params);
            params = next;
        }
        return NULL;
    }

    char* return_type = strndup(return_type_tok.lexeme, return_type_tok.len);
    if (!return_type)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for return type");
        error = true;
        while (params)
        {
            param_node_t* next = params->next;
            free(params->name);
            free(params->type);
            free(params);
            params = next;
        }
        return NULL;
    }
    char* name = strndup(name_tok.lexeme, name_tok.len);
    if (!name)
    {
        free(return_type);
        ERROR_FATAL("", 0, 0, "Memory allocation failed for function name");
        error = true;
        while (params)
        {
            param_node_t* next = params->next;
            free(params->name);
            free(params->type);
            free(params);
            params = next;
        }
        return NULL;
    }

    if (parser_peek(parser).type == TOKEN_SEMI)
    {
        parser_advance(parser);
        return ast_create_func_def(name, name_tok.len, return_type, params, NULL, true);
    }

    if (parser_peek(parser).type != TOKEN_LBRACE)
    {
        parser_error(parser, "Expected '{' for function body");
        free(name);
        free(return_type);
        while (params)
        {
            param_node_t* next = params->next;
            free(params->name);
            free(params->type);
            free(params);
            params = next;
        }
        return NULL;
    }
    parser_advance(parser);

    ast_node_t* stmts      = NULL;
    size_t      stmt_count = 0;
    size_t      capacity   = 0;

    while (parser_peek(parser).type != TOKEN_RBRACE)
    {
        ast_node_t* stmt = parse_statement(parser);
        if (error)
        {
            free(name);
            free(return_type);
            for (size_t i = 0; i < stmt_count; i++)
                ast_free_internal(&stmts[i]);
            free(stmts);
            while (params)
            {
                param_node_t* next = params->next;
                free(params->name);
                free(params->type);
                free(params);
                params = next;
            }
            return NULL;
        }
        if (!stmt)
            break;

        if (stmt_count >= capacity)
        {
            capacity              = capacity ? capacity * 2 : 8;
            ast_node_t* new_stmts = (ast_node_t*) realloc(stmts, capacity * sizeof(ast_node_t));
            if (!new_stmts)
            {
                ERROR_FATAL("", 0, 0, "Memory allocation failed for statements");
                error = true;
                ast_free(stmt);
                free(name);
                free(return_type);
                for (size_t i = 0; i < stmt_count; i++)
                    ast_free_internal(&stmts[i]);
                free(stmts);
                while (params)
                {
                    param_node_t* next = params->next;
                    free(params->name);
                    free(params->type);
                    free(params);
                    params = next;
                }
                return NULL;
            }
            stmts = new_stmts;
        }

        stmts[stmt_count++] = *stmt;
        free(stmt);
    }

    if (parser_peek(parser).type != TOKEN_RBRACE)
    {
        parser_error(parser, "Expected '}' to close function body");
        free(name);
        free(return_type);
        for (size_t i = 0; i < stmt_count; i++)
            ast_free_internal(&stmts[i]);
        free(stmts);
        while (params)
        {
            param_node_t* next = params->next;
            free(params->name);
            free(params->type);
            free(params);
            params = next;
        }
        return NULL;
    }
    parser_advance(parser);

    ast_node_t* block = ast_create_block(stmts, stmt_count);
    if (!block)
    {
        free(name);
        free(return_type);
        for (size_t i = 0; i < stmt_count; i++)
            ast_free_internal(&stmts[i]);
        free(stmts);
        while (params)
        {
            param_node_t* next = params->next;
            free(params->name);
            free(params->type);
            free(params);
            params = next;
        }
        return NULL;
    }

    return ast_create_func_def(name, name_tok.len, return_type, params, block, false);
}

static ast_node_t* parse_func_call(parser_t* parser)
{
    if (error)
        return NULL;
    token_t name_tok = parser_peek(parser);
    if (name_tok.type != TOKEN_IDENT)
    {
        parser_error(parser, "Expected identifier for function call");
        return NULL;
    }
    char* name = strndup(name_tok.lexeme, name_tok.len);
    if (!name)
    {
        ERROR_FATAL("", 0, 0, "Memory allocation failed for function name");
        error = true;
        return NULL;
    }
    parser_advance(parser);

    if (parser_peek(parser).type != TOKEN_LPAREN)
    {
        free(name);
        parser_error(parser, "Expected '(' for function call");
        return NULL;
    }
    parser_advance(parser);

    ast_node_t* args      = NULL;
    size_t      arg_count = 0;
    size_t      capacity  = 0;

    if (parser_peek(parser).type != TOKEN_RPAREN)
    {
        while (parser_peek(parser).type != TOKEN_RPAREN)
        {
            if (arg_count >= capacity)
            {
                capacity             = capacity ? capacity * 2 : 8;
                ast_node_t* new_args = (ast_node_t*) realloc(args, capacity * sizeof(ast_node_t));
                if (!new_args)
                {
                    ERROR_FATAL("", 0, 0, "Memory allocation failed for args");
                    error = true;
                    for (size_t i = 0; i < arg_count; i++)
                        ast_free_internal(&args[i]);
                    free(args);
                    free(name);
                    return NULL;
                }
                args = new_args;
            }

            ast_node_t* arg = parse_expression(parser, 0);
            if (error || !arg)
            {
                parser_error(parser, "Failed to parse function call argument");
                for (size_t i = 0; i < arg_count; i++)
                    ast_free_internal(&args[i]);
                free(args);
                free(name);
                return NULL;
            }
            args[arg_count++] = *arg;
            free(arg);

            if (parser_peek(parser).type == TOKEN_COMMA)
                parser_advance(parser);
            else if (parser_peek(parser).type != TOKEN_RPAREN)
            {
                parser_error(parser, "Expected ',' or ')' in argument list");
                for (size_t i = 0; i < arg_count; i++)
                    ast_free_internal(&args[i]);
                free(args);
                free(name);
                return NULL;
            }
        }
    }

    if (parser_peek(parser).type != TOKEN_RPAREN)
    {
        parser_error(parser, "Expected ')' to close argument list");
        for (size_t i = 0; i < arg_count; i++)
            ast_free_internal(&args[i]);
        free(args);
        free(name);
        return NULL;
    }
    parser_advance(parser);
    return ast_create_func_call(name, name_tok.len, args, arg_count);
}

static ast_node_t* parse_if_statement(parser_t* parser)
{
    if (error)
        return NULL;
    token_t tok = parser_peek(parser);
    if (tok.type != TOKEN_KEYWORD || strncmp(tok.lexeme, "if", tok.len) != 0)
    {
        parser_error(parser, "Expected 'if' keyword");
        return NULL;
    }
    parser_advance(parser);

    if (parser_peek(parser).type != TOKEN_LPAREN)
    {
        parser_error(parser, "Expected '(' after 'if'");
        return NULL;
    }
    parser_advance(parser);

    ast_node_t* condition = parse_expression(parser, 0);
    if (error || !condition)
    {
        parser_error(parser, "Expected condition expression in 'if'");
        return NULL;
    }

    if (parser_peek(parser).type != TOKEN_RPAREN)
    {
        parser_error(parser, "Expected ')' after condition");
        ast_free(condition);
        return NULL;
    }
    parser_advance(parser);

    if (parser_peek(parser).type != TOKEN_LBRACE)
    {
        parser_error(parser, "Expected '{' for if body");
        ast_free(condition);
        return NULL;
    }
    parser_advance(parser);

    ast_node_t* stmts      = NULL;
    size_t      stmt_count = 0;
    size_t      capacity   = 0;

    while (parser_peek(parser).type != TOKEN_RBRACE)
    {
        ast_node_t* stmt = parse_statement(parser);
        if (error)
        {
            ast_free(condition);
            for (size_t i = 0; i < stmt_count; i++)
                ast_free_internal(&stmts[i]);
            free(stmts);
            return NULL;
        }
        if (!stmt)
            break;

        if (stmt_count >= capacity)
        {
            capacity              = capacity ? capacity * 2 : 8;
            ast_node_t* new_stmts = (ast_node_t*) realloc(stmts, capacity * sizeof(ast_node_t));
            if (!new_stmts)
            {
                ERROR_FATAL("", 0, 0, "Memory allocation failed for statements");
                error = true;
                ast_free(stmt);
                ast_free(condition);
                for (size_t i = 0; i < stmt_count; i++)
                    ast_free_internal(&stmts[i]);
                free(stmts);
                return NULL;
            }
            stmts = new_stmts;
        }

        stmts[stmt_count++] = *stmt;
        free(stmt);
    }

    if (parser_peek(parser).type != TOKEN_RBRACE)
    {
        parser_error(parser, "Expected '}' to close if body");
        ast_free(condition);
        for (size_t i = 0; i < stmt_count; i++)
            ast_free_internal(&stmts[i]);
        free(stmts);
        return NULL;
    }
    parser_advance(parser);

    ast_node_t* then_block = ast_create_block(stmts, stmt_count);
    if (!then_block)
    {
        ast_free(condition);
        for (size_t i = 0; i < stmt_count; i++)
            ast_free_internal(&stmts[i]);
        free(stmts);
        return NULL;
    }

    ast_node_t* else_block = NULL;
    token_t     next_tok   = parser_peek(parser);
    if (next_tok.type == TOKEN_KEYWORD && strncmp(next_tok.lexeme, "else", next_tok.len) == 0)
    {
        parser_advance(parser);
        next_tok = parser_peek(parser);
        if (next_tok.type == TOKEN_KEYWORD && strncmp(next_tok.lexeme, "if", next_tok.len) == 0)
        {
            else_block = parse_if_statement(parser);
            if (error)
            {
                ast_free(condition);
                ast_free(then_block);
                return NULL;
            }
        }
        else if (next_tok.type == TOKEN_LBRACE)
        {
            parser_advance(parser);
            stmts      = NULL;
            stmt_count = 0;
            capacity   = 0;

            while (parser_peek(parser).type != TOKEN_RBRACE)
            {
                ast_node_t* stmt = parse_statement(parser);
                if (error)
                {
                    ast_free(condition);
                    ast_free(then_block);
                    for (size_t i = 0; i < stmt_count; i++)
                        ast_free_internal(&stmts[i]);
                    free(stmts);
                    return NULL;
                }
                if (!stmt)
                    break;

                if (stmt_count >= capacity)
                {
                    capacity = capacity ? capacity * 2 : 8;
                    ast_node_t* new_stmts =
                        (ast_node_t*) realloc(stmts, capacity * sizeof(ast_node_t));
                    if (!new_stmts)
                    {
                        ERROR_FATAL("", 0, 0, "Memory allocation failed for else statements");
                        error = true;
                        ast_free(stmt);
                        ast_free(condition);
                        ast_free(then_block);
                        for (size_t i = 0; i < stmt_count; i++)
                            ast_free_internal(&stmts[i]);
                        free(stmts);
                        return NULL;
                    }
                    stmts = new_stmts;
                }

                stmts[stmt_count++] = *stmt;
                free(stmt);
            }

            if (parser_peek(parser).type != TOKEN_RBRACE)
            {
                parser_error(parser, "Expected '}' to close else body");
                ast_free(condition);
                ast_free(then_block);
                for (size_t i = 0; i < stmt_count; i++)
                    ast_free_internal(&stmts[i]);
                free(stmts);
                return NULL;
            }
            parser_advance(parser);

            ast_node_t* else_body = ast_create_block(stmts, stmt_count);
            if (!else_body)
            {
                ast_free(condition);
                ast_free(then_block);
                for (size_t i = 0; i < stmt_count; i++)
                    ast_free_internal(&stmts[i]);
                free(stmts);
                return NULL;
            }
            else_block = ast_create_else(else_body);
            if (!else_block)
            {
                ast_free(condition);
                ast_free(then_block);
                ast_free(else_body);
                return NULL;
            }
        }
        else
        {
            parser_error(parser, "Expected 'if' or '{' after 'else'");
            ast_free(condition);
            ast_free(then_block);
            return NULL;
        }
    }

    return ast_create_if(condition, then_block, else_block);
}

static ast_node_t* parse_statement(parser_t* parser)
{
    if (error)
        return NULL;
    token_t tok = parser_peek(parser);

    if (tok.type == TOKEN_LBRACE)
    {
        parser_advance(parser);
        ast_node_t* stmts      = NULL;
        size_t      stmt_count = 0;
        size_t      capacity   = 0;

        while (parser_peek(parser).type != TOKEN_RBRACE)
        {
            ast_node_t* stmt = parse_statement(parser);
            if (error)
            {
                for (size_t i = 0; i < stmt_count; i++)
                    ast_free_internal(&stmts[i]);
                free(stmts);
                return NULL;
            }
            if (!stmt)
                break;

            if (stmt_count >= capacity)
            {
                capacity              = capacity ? capacity * 2 : 8;
                ast_node_t* new_stmts = (ast_node_t*) realloc(stmts, capacity * sizeof(ast_node_t));
                if (!new_stmts)
                {
                    ERROR_FATAL("", 0, 0, "Memory allocation failed for block statements");
                    error = true;
                    ast_free(stmt);
                    for (size_t i = 0; i < stmt_count; i++)
                        ast_free_internal(&stmts[i]);
                    free(stmts);
                    return NULL;
                }
                stmts = new_stmts;
            }

            stmts[stmt_count++] = *stmt;
            free(stmt);
        }

        if (parser_peek(parser).type != TOKEN_RBRACE)
        {
            parser_error(parser, "Expected '}' to close block");
            for (size_t i = 0; i < stmt_count; i++)
                ast_free_internal(&stmts[i]);
            free(stmts);
            return NULL;
        }
        parser_advance(parser);

        return ast_create_block(stmts, stmt_count);
    }
    else if (tok.type == TOKEN_KEYWORD && strncmp(tok.lexeme, "return", tok.len) == 0)
    {
        parser_advance(parser);
        ast_node_t* expr = parse_expression(parser, 0);
        if (error)
        {
            ast_free(expr);
            return NULL;
        }
        if (parser_peek(parser).type != TOKEN_SEMI)
        {
            parser_error(parser, "Expected ';' after return statement");
            ast_free(expr);
            return NULL;
        }
        parser_advance(parser);
        return ast_create_return(expr);
    }
    else if (tok.type == TOKEN_KEYWORD && strncmp(tok.lexeme, "import", tok.len) == 0)
    {
        parser_advance(parser);
        token_t ident_tok = parser_peek(parser);
        if (ident_tok.type != TOKEN_IDENT)
        {
            parser_error(parser, "Expected module name after import statement");
            return NULL;
        }

        size_t total_len   = ident_tok.len;
        size_t ident_count = 1;
        size_t temp_pos    = parser->pos;

        while (parser->tokens[temp_pos + 1].type == TOKEN_DOT &&
               parser->tokens[temp_pos + 2].type == TOKEN_IDENT)
        {
            total_len += 1;
            total_len += parser->tokens[temp_pos + 2].len;
            ident_count++;
            temp_pos += 2;
        }

        char* module = (char*) malloc(total_len + 1);
        if (!module)
        {
            ERROR_FATAL("", 0, 0, "Memory allocation failed for module name");
            error = true;
            return NULL;
        }
        module[0] = '\0';

        size_t offset = 0;
        for (size_t i = 0; i < ident_count; i++)
        {
            token_t current_ident = parser_peek(parser);
            if (current_ident.type != TOKEN_IDENT)
            {
                parser_error(parser, "Expected identifier in module name");
                free(module);
                return NULL;
            }
            memcpy(module + offset, current_ident.lexeme, current_ident.len);
            offset += current_ident.len;
            parser_advance(parser);

            if (i < ident_count - 1)
            {
                if (parser_peek(parser).type != TOKEN_DOT)
                {
                    parser_error(parser, "Expected '.' in module name");
                    free(module);
                    return NULL;
                }
                module[offset++] = '.';
                parser_advance(parser);
            }
        }
        module[offset] = '\0';

        if (parser_peek(parser).type != TOKEN_SEMI)
        {
            parser_error(parser, "Expected ';' after import statement");
            free(module);
            return NULL;
        }
        parser_advance(parser);

        return ast_create_import(module);
    }
    else if (tok.type == TOKEN_KEYWORD)
    {
        token_t type_tok = parser_advance(parser);
        token_t name_tok = parser_peek(parser);
        if (name_tok.type != TOKEN_IDENT)
        {
            parser_error(parser, "Expected identifier after type");
            return NULL;
        }
        parser_advance(parser);
        if (parser_peek(parser).type == TOKEN_LPAREN)
        {
            parser->pos -= 2;
            return parse_func_def(parser);
        }
        else if (parser_peek(parser).type == TOKEN_ASSIGN)
        {
            parser_advance(parser);
            ast_node_t* value = parse_expression(parser, 0);
            if (error || !value)
            {
                parser_error(parser, "Expected expression after '=' in definition");
                return NULL;
            }
            if (parser_peek(parser).type != TOKEN_SEMI)
            {
                parser_error(parser, "Expected ';' after definition");
                ast_free(value);
                return NULL;
            }
            parser_advance(parser);
            char* name = strndup(name_tok.lexeme, name_tok.len);
            char* type = strndup(type_tok.lexeme, type_tok.len);
            if (!name || !type)
            {
                free(name);
                free(type);
                ast_free(value);
                ERROR_FATAL("", 0, 0, "Memory allocation failed for definition");
                error = true;
                return NULL;
            }
            return ast_create_assign(name, name_tok.len, type, value);
        }
        else
        {
            parser_error(parser, "Expected '=' or '(' after identifier");
            return NULL;
        }
    }
    else if (tok.type == TOKEN_IDENT)
    {
        token_t name_tok = parser_advance(parser);
        if (parser_peek(parser).type == TOKEN_LPAREN)
        {
            parser->pos--;
            ast_node_t* call = parse_func_call(parser);
            if (error)
                return NULL;
            if (parser_peek(parser).type != TOKEN_SEMI)
            {
                parser_error(parser, "Expected ';' after function call");
                ast_free(call);
                return NULL;
            }
            parser_advance(parser);
            return call;
        }
        else if (parser_peek(parser).type == TOKEN_ASSIGN)
        {
            parser_advance(parser);
            ast_node_t* value = parse_expression(parser, 0);
            if (error || !value)
            {
                parser_error(parser, "Expected expression after '=' in assignment");
                return NULL;
            }
            if (parser_peek(parser).type != TOKEN_SEMI)
            {
                parser_error(parser, "Expected ';' after assignment");
                ast_free(value);
                return NULL;
            }
            parser_advance(parser);
            char* name = strndup(name_tok.lexeme, name_tok.len);
            if (!name)
            {
                ast_free(value);
                ERROR_FATAL("", 0, 0, "Memory allocation failed for assignment");
                error = true;
                return NULL;
            }
            return ast_create_assign(name, name_tok.len, NULL, value);
        }
        else
        {
            parser_error(parser, "Expected '=' or '(' after identifier");
            return NULL;
        }
    }

    parser_error(parser, "Unknown statement");
    return NULL;
}

/* ================== */
/* Main parser        */
/* ================== */
ast_node_t* ast_gen(token_t* tokens)
{
    parser_t parser = {.tokens = tokens, .pos = 0};
    error           = false;

    ast_node_t* func_defs      = NULL;
    size_t      func_def_count = 0;
    size_t      capacity       = 0;

    while (parser_peek(&parser).type != TOKEN_EOF)
    {
        ast_node_t* stmt = parse_statement(&parser);
        if (error)
        {
            for (size_t i = 0; i < func_def_count; i++)
                ast_free_internal(&func_defs[i]);
            free(func_defs);
            return NULL;
        }
        if (!stmt)
            break;

        if (stmt->type != NODE_FUNC_DEF && stmt->type != NODE_IMPORT)
        {
            parser_error(&parser, "Only function definitions and imports are allowed at top level");
            ast_free(stmt);
            for (size_t i = 0; i < func_def_count; i++)
                ast_free_internal(&func_defs[i]);
            free(func_defs);
            return NULL;
        }

        if (func_def_count >= capacity)
        {
            capacity = capacity ? capacity * 2 : 8;
            ast_node_t* new_func_defs =
                (ast_node_t*) realloc(func_defs, capacity * sizeof(ast_node_t));
            if (!new_func_defs)
            {
                ERROR_FATAL("", 0, 0, "Memory allocation failed for function definitions");
                error = true;
                ast_free(stmt);
                for (size_t i = 0; i < func_def_count; i++)
                    ast_free_internal(&func_defs[i]);
                free(func_defs);
                return NULL;
            }
            func_defs = new_func_defs;
        }

        func_defs[func_def_count++] = *stmt;
        free(stmt);
    }

    if (parser_peek(&parser).type != TOKEN_EOF)
    {
        parser_error(&parser, "Unexpected token after function definition");
        for (size_t i = 0; i < func_def_count; i++)
            ast_free_internal(&func_defs[i]);
        free(func_defs);
        return NULL;
    }

    return ast_create_program(func_defs, func_def_count);
}

static void ast_free_internal(ast_node_t* node)
{
    if (!node)
        return;
    switch (node->type)
    {
    case NODE_BINOP:
        ast_free(node->data.binop.left);
        ast_free(node->data.binop.right);
        break;
    case NODE_NUMBER:
        break;
    case NODE_STRING:
        if (node->data.string.value)
            free(node->data.string.value);
        break;
    case NODE_IDENT:
        if (node->data.ident.name)
            free(node->data.ident.name);
        break;
    case NODE_ASSIGN:
        if (node->data.assign.name)
            free(node->data.assign.name);
        if (node->data.assign.type)
            free(node->data.assign.type);
        ast_free(node->data.assign.value);
        break;
    case NODE_RETURN:
        ast_free(node->data.return_stmt.expr);
        break;
    case NODE_FUNC_DEF:
        if (node->data.func_def.name)
            free(node->data.func_def.name);
        if (node->data.func_def.return_type)
            free(node->data.func_def.return_type);
        param_node_t* param = node->data.func_def.params;
        while (param)
        {
            param_node_t* next = param->next;
            if (param->name)
                free(param->name);
            if (param->type)
                free(param->type);
            free(param);
            param = next;
        }
        ast_free(node->data.func_def.root);
        break;
    case NODE_FUNC_CALL:
        if (node->data.func_call.name)
            free(node->data.func_call.name);
        if (node->data.func_call.args)
        {
            for (size_t i = 0; i < node->data.func_call.arg_count; i++)
            {
                ast_free_internal(&node->data.func_call.args[i]);
            }
            free(node->data.func_call.args);
        }
        break;
    case NODE_BLOCK:
        if (node->data.block.stmts)
        {
            for (size_t i = 0; i < node->data.block.stmt_count; i++)
            {
                ast_free_internal(&node->data.block.stmts[i]);
            }
            free(node->data.block.stmts);
        }
        break;
    case NODE_PROGRAM:
        if (node->data.program.func_defs)
        {
            for (size_t i = 0; i < node->data.program.func_def_count; i++)
            {
                ast_free_internal(&node->data.program.func_defs[i]);
            }
            free(node->data.program.func_defs);
        }
        break;
    case NODE_IF:
        ast_free(node->data.if_stmt.condition);
        ast_free(node->data.if_stmt.then_block);
        ast_free(node->data.if_stmt.else_block);
        break;
    case NODE_ELSEIF:
        ast_free(node->data.elseif_stmt.condition);
        ast_free(node->data.elseif_stmt.then_block);
        ast_free(node->data.elseif_stmt.else_block);
        break;
    case NODE_ELSE:
        ast_free(node->data.else_stmt.block);
        break;
    case NODE_IMPORT:
        if (node->data.import.module)
            free(node->data.import.module);
        break;
    }
}

void ast_free(ast_node_t* node)
{
    if (!node)
        return;
    ast_free_internal(node);
    free(node);
}