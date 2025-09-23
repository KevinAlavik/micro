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
#include <parser.h>

static void print_ast_indent(ast_node_t* node, int indent_level)
{
    if (!node)
        return;

    for (int i = 0; i < indent_level; i++)
        printf("  ");

    switch (node->type)
    {
    case NODE_BINOP:
        printf("BinOp(%s,\n", TOKEN_TYPE_STR(node->data.binop.op));
        print_ast_indent(node->data.binop.left, indent_level + 1);
        printf(",\n");
        print_ast_indent(node->data.binop.right, indent_level + 1);
        printf("\n");
        for (int i = 0; i < indent_level; i++)
            printf("  ");
        printf(")");
        break;
    case NODE_NUMBER:
        if (node->data.number.lit_type == TOKEN_NLIT)
            printf("Number(%ld)", node->data.number.value.i64);
        else
            printf("Number(%.3f)", node->data.number.value.f64);
        break;
    case NODE_STRING:
        printf("String(\"%.*s\")", (int) node->data.string.len, node->data.string.value);
        break;
    case NODE_IDENT:
        printf("Ident(%.*s)", (int) node->data.ident.name_len, node->data.ident.name);
        break;
    case NODE_ASSIGN:
        if (node->data.assign.type)
            printf("Definition(%s, %.*s,\n", node->data.assign.type,
                   (int) node->data.assign.name_len, node->data.assign.name);
        else
            printf("Assignment(%.*s,\n", (int) node->data.assign.name_len, node->data.assign.name);
        print_ast_indent(node->data.assign.value, indent_level + 1);
        printf("\n");
        for (int i = 0; i < indent_level; i++)
            printf("  ");
        printf(")");
        break;
    case NODE_RETURN:
        printf("Return(\n");
        print_ast_indent(node->data.return_stmt.expr, indent_level + 1);
        printf("\n");
        for (int i = 0; i < indent_level; i++)
            printf("  ");
        printf(")");
        break;
    case NODE_FUNC_DEF:
        printf("FuncDef(%.*s, %s, ", (int) node->data.func_def.name_len, node->data.func_def.name,
               node->data.func_def.return_type);
        printf("[");
        param_node_t* param = node->data.func_def.params;
        while (param)
        {
            printf("%.*s: %s", (int) param->name_len, param->name, param->type);
            if (param->next)
                printf(", ");
            param = param->next;
        }
        printf("],\n");
        print_ast_indent(node->data.func_def.root, indent_level + 1);
        printf("\n");
        for (int i = 0; i < indent_level; i++)
            printf("  ");
        printf(")");
        break;
    case NODE_FUNC_CALL:
        printf("FuncCall(%.*s, ", (int) node->data.func_call.name_len, node->data.func_call.name);
        printf("[");
        for (size_t i = 0; i < node->data.func_call.arg_count; i++)
        {
            printf("\n");
            print_ast_indent(&node->data.func_call.args[i], indent_level + 2);
            if (i < node->data.func_call.arg_count - 1)
                printf(",");
        }
        printf("\n");
        for (int i = 0; i < indent_level; i++)
            printf("  ");
        printf("])");
        break;
    case NODE_BLOCK:
        printf("Block([\n");
        for (size_t i = 0; i < node->data.block.stmt_count; i++)
        {
            print_ast_indent(&node->data.block.stmts[i], indent_level + 1);
            if (i < node->data.block.stmt_count - 1)
                printf(",");
            printf("\n");
        }
        for (int i = 0; i < indent_level; i++)
            printf("  ");
        printf("])");
        break;
    case NODE_PROGRAM:
        printf("Program([\n");
        for (size_t i = 0; i < node->data.program.func_def_count; i++)
        {
            print_ast_indent(&node->data.program.func_defs[i], indent_level + 1);
            if (i < node->data.program.func_def_count - 1)
                printf(",");
            printf("\n");
        }
        for (int i = 0; i < indent_level; i++)
            printf("  ");
        printf("])");
        break;
    case NODE_IF:
        printf("If(\n");
        print_ast_indent(node->data.if_stmt.condition, indent_level + 1);
        printf(",\n");
        print_ast_indent(node->data.if_stmt.then_block, indent_level + 1);
        if (node->data.if_stmt.else_block)
        {
            printf(",\n");
            print_ast_indent(node->data.if_stmt.else_block, indent_level + 1);
        }
        printf("\n");
        for (int i = 0; i < indent_level; i++)
            printf("  ");
        printf(")");
        break;
    case NODE_ELSEIF:
        printf("ElseIf(\n");
        print_ast_indent(node->data.elseif_stmt.condition, indent_level + 1);
        printf(",\n");
        print_ast_indent(node->data.elseif_stmt.then_block, indent_level + 1);
        if (node->data.elseif_stmt.else_block)
        {
            printf(",\n");
            print_ast_indent(node->data.elseif_stmt.else_block, indent_level + 1);
        }
        printf("\n");
        for (int i = 0; i < indent_level; i++)
            printf("  ");
        printf(")");
        break;
    case NODE_ELSE:
        printf("Else(\n");
        print_ast_indent(node->data.else_stmt.block, indent_level + 1);
        printf("\n");
        for (int i = 0; i < indent_level; i++)
            printf("  ");
        printf(")");
        break;
    }
}

static void print_ast(ast_node_t* node)
{
    print_ast_indent(node, 0);
}

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
            free(tokens);
            free(source);
            return 1;
        }

        if (count >= capacity)
        {
            capacity *= 2;
            token_t* new_tokens = realloc(tokens, capacity * sizeof(token_t));
            if (!new_tokens)
            {
                fprintf(stderr, "Memory allocation failed\n");
                free(tokens);
                free(source);
                return 1;
            }
            tokens = new_tokens;
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

    ast_node_t* ast = ast_gen(tokens);
    if (ast)
    {
        printf("\n=== AST ===\n");
        print_ast(ast);
        printf("\n");
        ast_free(ast);
    }

    for (size_t i = 0; i < count; i++)
    {
        if (tokens[i].type == TOKEN_SLIT)
        {
            free((char*) tokens[i].value.str.x);
        }
    }
    free(tokens);
    free(source);
    return 0;
}