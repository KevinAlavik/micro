/*
 * cmicro - Micro compiler
 * Copyright (C) 2025 Kevin Alavik <kevin@alavik.se>
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <codegen.h>
#include <stdio.h>
#include <stdlib.h>
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>

int codegen_generate(ast_node_t* root, const char* output_path)
{
    if (!root || root->type != NODE_PROGRAM)
    {
        fprintf(stderr, "Error: Root node must be a program\n");
        return 1;
    }

    LLVMModuleRef     module    = LLVMModuleCreateWithName("test");
    LLVMTypeRef       main_type = LLVMFunctionType(LLVMInt32Type(), NULL, 0, 0);
    LLVMValueRef      main_func = LLVMAddFunction(module, "main", main_type);
    LLVMBasicBlockRef entry     = LLVMAppendBasicBlock(main_func, "entry");
    LLVMBuilderRef    builder   = LLVMCreateBuilder();
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildRet(builder, LLVMConstInt(LLVMInt32Type(), 69, 0));

    char* error = NULL;
    if (LLVMVerifyModule(module, LLVMAbortProcessAction, &error))
    {
        fprintf(stderr, "Module verification error: %s\n", error);
        LLVMDisposeMessage(error);
        LLVMDisposeBuilder(builder);
        LLVMDisposeModule(module);
        return 1;
    }
    LLVMDisposeMessage(error);

    if (LLVMWriteBitcodeToFile(module, "test.bc") != 0)
    {
        fprintf(stderr, "Error writing bitcode\n");
        LLVMDisposeBuilder(builder);
        LLVMDisposeModule(module);
        return 1;
    }

    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(module);

    char command[4096];
    snprintf(command, sizeof(command), "clang test.bc -o %s", output_path);
    int compile_result = system(command);
    if (compile_result != 0)
    {
        fprintf(stderr, "Error compiling bitcode to %s\n", output_path);
        remove("test.bc");
        return 1;
    }

    remove("test.bc");

    return 0;
}