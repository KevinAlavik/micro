/*
 * cmicro - Micro compiler
 * Copyright (C) 2025 Kevin Alavik <kevin@alavik.se>
 *
 * Licensed under the Apache License, Version 2.0
 */
#ifndef _CMICRO_CODEGEN_H
#define _CMICRO_CODEGEN_H

#include <parser.h>
#include <stdio.h>

int codegen_generate(ast_node_t* root, const char* output_path);

#endif // _CMICRO_CODEGEN_H