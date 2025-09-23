/*
 * cmicro - Micro compiler
 * Copyright (C) 2025 Kevin Alavik <kevin@alavik.se>
 *
 * Licensed under the Apache License, Version 2.0
 */

#define _GNU_SOURCE
#include <codegen.h>
#include <parser.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

typedef struct gen_result
{
    char* val;
    char  qbe_type;
} gen_result_t;

typedef struct var_info
{
    char* ptr;
    char  vtype;
} var_info_t;

typedef struct sym_entry
{
    char*             name;
    char*             ptr;
    char              vtype;
    struct sym_entry* next;
} sym_entry_t;

typedef struct scope
{
    sym_entry_t*  entries;
    struct scope* prev;
} scope_t;

typedef struct func_entry
{
    char*              name;
    ast_node_t*        node;
    struct func_entry* next;
} func_entry_t;

typedef struct str_info
{
    char*            value;
    size_t           len;
    char*            gname;
    struct str_info* next;
} str_info_t;

typedef struct codegen_context
{
    FILE*         out;
    scope_t*      current_scope;
    func_entry_t* funcs;
    str_info_t*   strings;
    int           temp_count;
    int           label_count;
    int           str_count;
} codegen_context_t;

static codegen_context_t ctx = {0};

static void         emit(const char* fmt, ...);
static char*        new_temp(void);
static char*        new_label(void);
static char         str_to_qbe_type(const char* s);
static void         push_scope(void);
static void         free_scope(scope_t* scope);
static void         pop_scope(void);
static void         add_sym(const char* name, size_t name_len, char* ptr, char vtype);
static var_info_t   find_sym(const char* name);
static ast_node_t*  find_func(const char* name);
static void         free_strings(void);
static void         collect_strings(ast_node_t* node);
static gen_result_t gen_expr(ast_node_t* node);
static void         gen_conditional(ast_node_t* node, char* cont_lab);
static void         gen_stmt(ast_node_t* node);
static gen_result_t gen_binop(token_type_t op, gen_result_t left, gen_result_t right);
static void         gen_block(ast_node_t* node);
static void         gen_func_def(ast_node_t* node);
static void         gen_program(ast_node_t* node);
static void         free_context(void);

static void emit(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(ctx.out, fmt, args);
    va_end(args);
}

static char* new_temp(void)
{
    char* buf = malloc(20);
    sprintf(buf, "%%t%d", ctx.temp_count++);
    return buf;
}

static char* new_label(void)
{
    char* buf = malloc(20);
    sprintf(buf, "@l%d", ctx.label_count++);
    return buf;
}

static char str_to_qbe_type(const char* s)
{
    static const struct
    {
        const char* name;
        char        qbe_type;
    } type_map[] = {{"int", 'l'}, {"float", 'd'}, {"string", 'l'}, {NULL, 0}};
    if (!s) // Handle NULL type for variadic parameters
        return 'l';
    for (int i = 0; type_map[i].name; i++)
    {
        if (strcmp(s, type_map[i].name) == 0)
            return type_map[i].qbe_type;
    }
    ERROR_FATAL(NULL, 0, 0, "Unknown type");
    return 'l';
}

static void push_scope(void)
{
    scope_t* new_scope = calloc(1, sizeof(scope_t));
    new_scope->prev    = ctx.current_scope;
    ctx.current_scope  = new_scope;
}

static void free_scope(scope_t* scope)
{
    sym_entry_t* entry = scope->entries;
    while (entry)
    {
        sym_entry_t* next = entry->next;
        free(entry->name);
        free(entry->ptr);
        free(entry);
        entry = next;
    }
    free(scope);
}

static void pop_scope(void)
{
    scope_t* scope    = ctx.current_scope;
    ctx.current_scope = scope->prev;
    free_scope(scope);
}

static void add_sym(const char* name, size_t name_len, char* ptr, char vtype)
{
    sym_entry_t* e             = calloc(1, sizeof(sym_entry_t));
    e->name                    = strndup(name, name_len);
    e->ptr                     = ptr;
    e->vtype                   = vtype;
    e->next                    = ctx.current_scope->entries;
    ctx.current_scope->entries = e;
}

static var_info_t find_sym(const char* name)
{
    for (scope_t* s = ctx.current_scope; s; s = s->prev)
    {
        for (sym_entry_t* e = s->entries; e; e = e->next)
        {
            if (strcmp(e->name, name) == 0)
            {
                return (var_info_t){e->ptr, e->vtype};
            }
        }
    }
    ERROR_FATAL(NULL, 0, 0, "Undefined variable");
    return (var_info_t){NULL, 0};
}

static ast_node_t* find_func(const char* name)
{
    for (func_entry_t* f = ctx.funcs; f; f = f->next)
    {
        if (strcmp(f->name, name) == 0)
            return f->node;
    }
    ERROR_WARN(NULL, 0, 0, "Function not found");
    return NULL;
}

static void free_strings(void)
{
    str_info_t* si = ctx.strings;
    while (si)
    {
        str_info_t* next = si->next;
        free(si->value);
        free(si->gname);
        free(si);
        si = next;
    }
    ctx.strings = NULL;
}

static void collect_strings(ast_node_t* node)
{
    if (!node)
        return;
    switch (node->type)
    {
    case NODE_PROGRAM:
        for (size_t i = 0; i < node->data.program.func_def_count; i++)
        {
            collect_strings(&node->data.program.func_defs[i]);
        }
        break;
    case NODE_FUNC_DEF:
        if (!node->data.func_def.is_declaration)
            collect_strings(node->data.func_def.root);
        break;
    case NODE_BLOCK:
        for (size_t i = 0; i < node->data.block.stmt_count; i++)
        {
            collect_strings(&node->data.block.stmts[i]);
        }
        break;
    case NODE_RETURN:
        collect_strings(node->data.return_stmt.expr);
        break;
    case NODE_FUNC_CALL:
        for (size_t i = 0; i < node->data.func_call.arg_count; i++)
        {
            collect_strings(&node->data.func_call.args[i]);
        }
        break;
    case NODE_ASSIGN:
        collect_strings(node->data.assign.value);
        break;
    case NODE_IF:
        collect_strings(node->data.if_stmt.condition);
        collect_strings(node->data.if_stmt.then_block);
        collect_strings(node->data.if_stmt.else_block);
        break;
    case NODE_ELSEIF:
        collect_strings(node->data.elseif_stmt.condition);
        collect_strings(node->data.elseif_stmt.then_block);
        collect_strings(node->data.elseif_stmt.else_block);
        break;
    case NODE_ELSE:
        collect_strings(node->data.else_stmt.block);
        break;
    case NODE_STRING:
    {
        char*  value = node->data.string.value;
        size_t len   = node->data.string.len;
        for (str_info_t* si = ctx.strings; si; si = si->next)
        {
            if (si->len == len && memcmp(si->value, value, len) == 0)
            {
                return;
            }
        }
        char* gname = malloc(10);
        sprintf(gname, "$str%d", ctx.str_count++);
        str_info_t* new_si = calloc(1, sizeof(str_info_t));
        new_si->value      = malloc(len);
        memcpy(new_si->value, value, len);
        new_si->len   = len;
        new_si->gname = gname;
        new_si->next  = ctx.strings;
        ctx.strings   = new_si;
        break;
    }
    default:
        break;
    }
}

static void gen_conditional(ast_node_t* node, char* cont_lab)
{
    int manage_cont = (cont_lab == NULL);
    if (manage_cont)
    {
        cont_lab = new_label();
    }

    gen_result_t cond = gen_expr(node->type == NODE_IF ? node->data.if_stmt.condition
                                                       : node->data.elseif_stmt.condition);
    if (!cond.val)
    {
        if (manage_cont)
            free(cont_lab);
        return;
    }
    char* then_lab = new_label();
    char* next_lab = new_label();
    emit("jnz %s, %s, %s\n", cond.val, then_lab, next_lab);
    free(cond.val);

    emit("%s\n", then_lab);
    gen_block(node->type == NODE_IF ? node->data.if_stmt.then_block
                                    : node->data.elseif_stmt.then_block);
    emit("jmp %s\n", cont_lab);

    emit("%s\n", next_lab);
    ast_node_t* else_block =
        node->type == NODE_IF ? node->data.if_stmt.else_block : node->data.elseif_stmt.else_block;
    if (else_block)
    {
        if (else_block->type == NODE_ELSEIF)
        {
            gen_conditional(else_block, cont_lab);
        }
        else if (else_block->type == NODE_ELSE)
        {
            gen_block(else_block->data.else_stmt.block);
            emit("jmp %s\n", cont_lab);
        }
    }

    if (manage_cont)
    {
        emit("%s\n", cont_lab);
        free(cont_lab);
    }
}

static void gen_stmt(ast_node_t* node)
{
    if (!node)
        return;
    switch (node->type)
    {
    case NODE_RETURN:
    {
        if (node->data.return_stmt.expr)
        {
            gen_result_t val = gen_expr(node->data.return_stmt.expr);
            if (val.val)
            {
                emit("ret %s\n", val.val);
                free(val.val);
            }
        }
        else
        {
            emit("ret\n");
        }
        break;
    }
    case NODE_FUNC_CALL:
    {
        gen_result_t res = gen_expr(node);
        if (res.val)
            free(res.val);
        break;
    }
    case NODE_ASSIGN:
    {
        gen_result_t res = gen_expr(node);
        if (res.val)
            free(res.val);
        break;
    }
    case NODE_IF:
        gen_conditional(node, NULL);
        break;
    case NODE_IMPORT:
        // Imports should be handled during parsing (its not)
        break;
    case NODE_BLOCK:
        gen_block(node);
        break;
    default:
        ERROR_FATAL(NULL, 0, 0, "Unimplemented statement type");
        break;
    }
}

static gen_result_t gen_binop(token_type_t op, gen_result_t left, gen_result_t right)
{
    gen_result_t res = {NULL, 0};
    if (!left.val || !right.val)
        return res;

    struct
    {
        token_type_t token;
        const char*  opstr;
        int          is_comp;
    } ops[] = {{TOKEN_PLUS, "add", 0},  {TOKEN_MINUS, "sub", 0},   {TOKEN_STAR, "mul", 0},
               {TOKEN_SLASH, "div", 0}, {TOKEN_PERCENT, "rem", 0}, {TOKEN_EQ, "ceq", 1},
               {TOKEN_NEQ, "cne", 1},   {TOKEN_LT, "slt", 1},      {TOKEN_LTE, "sle", 1},
               {TOKEN_GT, "sgt", 1},    {TOKEN_GTE, "sge", 1},     {0, NULL, 0}};

    const char* opstr   = NULL;
    int         is_comp = 0;
    for (int i = 0; ops[i].opstr; i++)
    {
        if (ops[i].token == op)
        {
            opstr   = ops[i].opstr;
            is_comp = ops[i].is_comp;
            break;
        }
    }
    if (!opstr)
    {
        ERROR_FATAL(NULL, 0, 0, "Unimplemented binary operator");
        free(left.val);
        free(right.val);
        return res;
    }

    char  otype   = left.qbe_type;
    char* tmp     = new_temp();
    char* full_op = malloc(strlen(opstr) + 2);
    sprintf(full_op, "%s%c", opstr, otype);
    if (is_comp)
    {
        emit("%s =w %s %s, %s\n", tmp, full_op, left.val, right.val);
        res.qbe_type = 'w';
    }
    else
    {
        emit("%s =%c %s %s, %s\n", tmp, otype, opstr, left.val, right.val);
        res.qbe_type = otype;
    }
    res.val = tmp;
    free(full_op);
    free(left.val);
    free(right.val);
    return res;
}

static gen_result_t gen_expr(ast_node_t* node)
{
    gen_result_t res = {NULL, 0};
    if (!node)
        return res;

    switch (node->type)
    {
    case NODE_NUMBER:
    {
        if (node->data.number.lit_type == TOKEN_NLIT)
        {
            char* buf = malloc(32);
            sprintf(buf, "%ld", node->data.number.value.i64);
            res.val      = buf;
            res.qbe_type = 'l';
        }
        else
        {
            char* buf = malloc(32);
            sprintf(buf, "d_%g", node->data.number.value.f64);
            res.val      = buf;
            res.qbe_type = 'd';
        }
        break;
    }
    case NODE_STRING:
    {
        char*  value = node->data.string.value;
        size_t len   = node->data.string.len;
        for (str_info_t* si = ctx.strings; si; si = si->next)
        {
            if (si->len == len && memcmp(si->value, value, len) == 0)
            {
                res.val      = strdup(si->gname);
                res.qbe_type = 'l';
                break;
            }
        }
        if (!res.val)
            ERROR_FATAL(NULL, 0, 0, "String not collected");
        break;
    }
    case NODE_IDENT:
    {
        var_info_t vi = find_sym(node->data.ident.name);
        if (!vi.ptr)
            break;
        res.val      = strdup(vi.ptr);
        res.qbe_type = vi.vtype;
        break;
    }
    case NODE_BINOP:
    {
        gen_result_t left  = gen_expr(node->data.binop.left);
        gen_result_t right = gen_expr(node->data.binop.right);
        res                = gen_binop(node->data.binop.op, left, right);
        break;
    }
    case NODE_FUNC_CALL:
    {
        char*       name        = strndup(node->data.func_call.name, node->data.func_call.name_len);
        ast_node_t* func        = find_func(name);
        char        ret_type    = func ? str_to_qbe_type(func->data.func_def.return_type) : 'l';
        bool        is_variadic = false;
        if (func)
        {
            for (param_node_t* param = func->data.func_def.params; param; param = param->next)
            {
                if (param->is_variadic)
                {
                    is_variadic = true;
                    break;
                }
            }
        }
        char* tmp = NULL;
        if (ret_type != 0)
        {
            tmp = new_temp();
            emit("%s =%c call $%s (", tmp, ret_type, name);
        }
        else
        {
            emit("call $%s (", name);
        }
        param_node_t* param       = func ? func->data.func_def.params : NULL;
        size_t        param_count = 0;
        for (param_node_t* p = param; p && !p->is_variadic; p = p->next)
            param_count++;

        for (size_t i = 0; i < node->data.func_call.arg_count; i++)
        {
            gen_result_t arg = gen_expr(&node->data.func_call.args[i]);
            if (!arg.val)
            {
                free(name);
                if (tmp)
                    free(tmp);
                return res;
            }
            char ptype =
                (param && i < param_count && !is_variadic) ? str_to_qbe_type(param->type) : 'l';
            emit("%c %s", ptype, arg.val);
            if (i < node->data.func_call.arg_count - 1)
                emit(", ");
            free(arg.val);
            if (param && i < param_count - 1)
                param = param->next;
        }
        emit(")\n");
        free(name);
        if (ret_type != 0)
        {
            res.val      = tmp;
            res.qbe_type = ret_type;
        }
        break;
    }
    case NODE_ASSIGN:
    {
        char*        name = strndup(node->data.assign.name, node->data.assign.name_len);
        gen_result_t val  = {NULL, 0};
        if (node->data.assign.value)
        {
            val = gen_expr(node->data.assign.value);
            if (!val.val)
            {
                free(name);
                return res;
            }
        }
        if (node->data.assign.type)
        {
            char  vtype      = str_to_qbe_type(node->data.assign.type);
            char* ptr        = new_temp();
            int   alloc_size = (vtype == 'w' || vtype == 's') ? 4 : 8;
            emit("%s =l alloc%d\n", ptr, alloc_size);
            add_sym(name, node->data.assign.name_len, ptr, vtype);
            if (node->data.assign.value)
            {
                emit("store%c %s, %s\n", vtype, ptr, val.val);
                res.val      = strdup(val.val);
                res.qbe_type = vtype;
            }
            else
            {
                emit("store%c %s, 0\n", vtype, ptr);
                res.val      = strdup("0");
                res.qbe_type = vtype;
            }
        }
        else
        {
            var_info_t vi = find_sym(name);
            if (!vi.ptr)
            {
                free(name);
                if (val.val)
                    free(val.val);
                break;
            }
            emit("store%c %s, %s\n", vi.vtype, vi.ptr, val.val);
            res.val      = strdup(val.val);
            res.qbe_type = vi.vtype;
        }
        free(name);
        if (val.val)
            free(val.val);
        break;
    }
    default:
        ERROR_FATAL(NULL, 0, 0, "Unimplemented expression type");
        break;
    }
    return res;
}

static void gen_block(ast_node_t* node)
{
    if (!node || node->type != NODE_BLOCK)
        return;
    push_scope();
    for (size_t i = 0; i < node->data.block.stmt_count; i++)
    {
        gen_stmt(&node->data.block.stmts[i]);
    }
    pop_scope();
}

static void gen_func_def(ast_node_t* node)
{
    if (!node || node->type != NODE_FUNC_DEF)
        return;
    if (node->data.func_def.is_declaration)
        return;

    char* name     = strndup(node->data.func_def.name, node->data.func_def.name_len);
    char  ret_type = str_to_qbe_type(node->data.func_def.return_type);
    if (strcmp(name, "main") == 0)
    {
        emit("export ");
    }
    emit("function ");
    if (ret_type != 0)
        emit("%c ", ret_type);
    emit("$%s (", name);
    param_node_t* param = node->data.func_def.params;
    while (param)
    {
        if (param->is_variadic)
        {
            emit("...");
            if (param->next)
                emit(", ");
        }
        else
        {
            char ptype = str_to_qbe_type(param->type);
            emit("%c %%%.*s", ptype, (int) param->name_len, param->name);
            if (param->next)
                emit(", ");
        }
        param = param->next;
    }
    emit(") {\n@start\n");
    free(name);

    push_scope();
    param = node->data.func_def.params;
    while (param && !param->is_variadic)
    {
        char  ptype      = str_to_qbe_type(param->type);
        char* param_name = strndup(param->name, param->name_len);
        char* param_ptr  = malloc(param->name_len + 2);
        sprintf(param_ptr, "%%%.*s", (int) param->name_len, param->name);
        add_sym(param->name, param->name_len, param_ptr, ptype);
        free(param_name);
        param = param->next;
    }
    gen_block(node->data.func_def.root);
    pop_scope();
    emit("}\n");
}

static void gen_program(ast_node_t* node)
{
    if (!node || node->type != NODE_PROGRAM)
        return;
    for (size_t i = 0; i < node->data.program.func_def_count; i++)
    {
        gen_func_def(&node->data.program.func_defs[i]);
    }
}

static void free_context(void)
{
    free_strings();
    while (ctx.funcs)
    {
        func_entry_t* next = ctx.funcs->next;
        free(ctx.funcs->name);
        free(ctx.funcs);
        ctx.funcs = next;
    }
    while (ctx.current_scope)
    {
        pop_scope();
    }
    ctx.temp_count  = 0;
    ctx.label_count = 0;
    ctx.str_count   = 0;
}

int codegen_generate(ast_node_t* root, const char* output_path)
{
    if (!root || root->type != NODE_PROGRAM)
    {
        ERROR_FATAL(NULL, 0, 0, "Root node must be a program");
        return 1;
    }

    char* qbe_path = malloc(strlen(output_path) + 5);
    char* asm_path = malloc(strlen(output_path) + 5);
    sprintf(qbe_path, "%s.qbe", output_path);
    sprintf(asm_path, "%s.asm", output_path);

    ctx.out = fopen(qbe_path, "w");
    if (!ctx.out)
    {
        ERROR_FATAL(NULL, 0, 0, "Failed to open QBE output file");
        free(qbe_path);
        free(asm_path);
        return 1;
    }

    collect_strings(root);

    for (str_info_t* si = ctx.strings; si; si = si->next)
    {
        emit("data %s = { ", si->gname);
        for (size_t j = 0; j < si->len; j++)
        {
            emit("b %d, ", (unsigned char) si->value[j]);
        }
        emit("b 0 }\n");
    }

    for (size_t i = 0; i < root->data.program.func_def_count; i++)
    {
        func_entry_t* fe = calloc(1, sizeof(func_entry_t));
        fe->name         = strndup(root->data.program.func_defs[i].data.func_def.name,
                                   root->data.program.func_defs[i].data.func_def.name_len);
        fe->node         = &root->data.program.func_defs[i];
        fe->next         = ctx.funcs;
        ctx.funcs        = fe;
    }

    gen_program(root);

    fclose(ctx.out);
    ctx.out = NULL;

    char* qbe_cmd = malloc(strlen(qbe_path) + strlen(asm_path) + 20);
    sprintf(qbe_cmd, "qbe -o %s %s", asm_path, qbe_path);
    int qbe_result = system(qbe_cmd);
    free(qbe_cmd);
    if (qbe_result != 0)
    {
        ERROR_FATAL(NULL, 0, 0, "QBE failed to generate assembly");
        unlink(qbe_path);
        free_context();
        free(qbe_path);
        free(asm_path);
        return 1;
    }

    char* cc_cmd = malloc(strlen(asm_path) + strlen(output_path) + 30);
    sprintf(cc_cmd, "clang -o %s %s", output_path, asm_path);
    int cc_result = system(cc_cmd);
    free(cc_cmd);
    if (cc_result != 0)
    {
        ERROR_FATAL(NULL, 0, 0, "Clang failed to link executable");
        unlink(qbe_path);
        unlink(asm_path);
        free_context();
        free(qbe_path);
        free(asm_path);
        return 1;
    }

    unlink(qbe_path);
    unlink(asm_path);

    free_context();
    free(qbe_path);
    free(asm_path);
    return 0;
}