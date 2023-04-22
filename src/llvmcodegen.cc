#include "llvmcodegen.hh"
#include "ast.hh"
#include <iostream>
#include <llvm/Support/FileSystem.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <vector>

#define MAIN_FUNC compiler->module.getFunction("main")

/*
The documentation for LLVM codegen, and how exactly this file works can be found
ins `docs/llvm.md`
*/
extern int yyerror(std::string msg);

int get_data_type(LLVMCompiler *compiler, Value* val)
{
    if(val->getType()->isIntegerTy(16))
        return 1;
    if(val->getType()->isIntegerTy(32))
        return 2;
    return 3;
}

void LLVMCompiler::compile(Node *root) {
    /* Adding reference to print_i in the runtime library */
    // void printi();
    FunctionType *printi_func_type = FunctionType::get(
        builder.getVoidTy(),
        {builder.getInt64Ty()},
        false
    );
    Function::Create(
        printi_func_type,
        GlobalValue::ExternalLinkage,
        "printi",
        &module
    );
    /* we can get this later 
        module.getFunction("printi");
    */

    /* Main Function */
    // int main();
    FunctionType *main_func_type = FunctionType::get(
        builder.getInt64Ty(), {}, false /* is vararg */
    );
    Function *main_func = Function::Create(
        main_func_type,
        GlobalValue::ExternalLinkage,
        "main",
        &module
    );

    // create main function block
    BasicBlock *main_func_entry_bb = BasicBlock::Create(
        *context,
        "entry",
        main_func
    );

    // move the builder to the start of the main function block
    builder.SetInsertPoint(main_func_entry_bb);

    root->llvm_codegen(this);

    // return 0;
    builder.CreateRet(builder.getInt64(0));
}

void LLVMCompiler::dump() {
    outs() << module;
}

void LLVMCompiler::write(std::string file_name) {
    std::error_code EC;
    raw_fd_ostream fout(file_name, EC, sys::fs::OF_None);
    WriteBitcodeToFile(module, fout);
    fout.flush();
    fout.close();
}

//  ┌―――――――――――――――――――――┐  //
//  │ AST -> LLVM Codegen │  //
// └―――――――――――――――――――――┘   //

// codegen for statements
Value *NodeStmts::llvm_codegen(LLVMCompiler *compiler) {
    Value *last = nullptr;
    for(auto node : list) {
        last = node->llvm_codegen(compiler);
    }

    return last;
}

Value *NodeDebug::llvm_codegen(LLVMCompiler *compiler) {
    Value *expr = expression->llvm_codegen(compiler);

    Function *printi_func = compiler->module.getFunction("printi");
    expr = compiler->builder.CreateIntCast(expr, compiler->builder.getInt64Ty(), 1, "final");
    compiler->builder.CreateCall(printi_func, {expr});

    return expr;
}

Value *NodeInt::llvm_codegen(LLVMCompiler *compiler) {
    if(data_type == 1)
        return compiler->builder.getInt16(value);
    else if(data_type == 2)
        return compiler->builder.getInt32(value);
    return compiler->builder.getInt64(value);
}

Value *NodeBinOp::llvm_codegen(LLVMCompiler *compiler) {
    Value *left_expr = left->llvm_codegen(compiler);
    Value *right_expr = right->llvm_codegen(compiler);
    int temp = std::max(get_data_type(compiler, left_expr), get_data_type(compiler, right_expr));
    if(temp == 1)
    {
        compiler->builder.CreateIntCast(left_expr, compiler->builder.getInt16Ty(), 1, "name1");
        compiler->builder.CreateIntCast(right_expr, compiler->builder.getInt16Ty(), 1, "name2");
    }
    else if(temp == 2)
    {
        compiler->builder.CreateIntCast(left_expr, compiler->builder.getInt32Ty(), 1, "name3");
        compiler->builder.CreateIntCast(right_expr, compiler->builder.getInt32Ty(), 1, "name4");
    }
    else
    {
        compiler->builder.CreateIntCast(left_expr, compiler->builder.getInt64Ty(), 1, "name5");
        compiler->builder.CreateIntCast(right_expr, compiler->builder.getInt64Ty(), 1, "name6");
    }

    switch(op) {
        case PLUS:
        return compiler->builder.CreateAdd(left_expr, right_expr, "addtmp");
        case MINUS:
        return compiler->builder.CreateSub(left_expr, right_expr, "minustmp");
        case MULT:
        return compiler->builder.CreateMul(left_expr, right_expr, "multtmp");
        case DIV:
        return compiler->builder.CreateSDiv(left_expr, right_expr, "divtmp");
    }
}


Value *NodeDecl::llvm_codegen(LLVMCompiler *compiler) {
    Value *expr = expression->llvm_codegen(compiler);
    IRBuilder<> temp_builder(
        &MAIN_FUNC->getEntryBlock(),
        MAIN_FUNC->getEntryBlock().begin()
    );


    AllocaInst* alloc;
    if(data_type == 1)
        alloc = temp_builder.CreateAlloca(compiler->builder.getInt16Ty(), 0, identifier);
    else if(data_type == 2)
        alloc = temp_builder.CreateAlloca(compiler->builder.getInt32Ty(), 0, identifier);
    else
        alloc = temp_builder.CreateAlloca(compiler->builder.getInt64Ty(), 0, identifier);

    std::string type_str1;
    raw_string_ostream rso(type_str1);
    expr->getType()->print(rso);
    if(type_str1 == "i16" && data_type == 2)
        expr = compiler->builder.CreateIntCast(expr, compiler->builder.getInt32Ty(), 1, "short to int");
    else if(type_str1 == "i16" && data_type == 3)
        expr = compiler->builder.CreateIntCast(expr, compiler->builder.getInt64Ty(), 1, "short to long");
    else if(type_str1 == "i32" && data_type == 3)
        expr = compiler->builder.CreateIntCast(expr, compiler->builder.getInt64Ty(), 1, "int to long");
    else if((type_str1 == "i16" && data_type > 1) || (type_str1 == "i32" && data_type > 2))
        yyerror("Type Mismatch");
    
    std::string type_str;
    raw_string_ostream rso1(type_str);
    expr->getType()->print(rso1);
    compiler->locals[identifier] = alloc;
    return compiler->builder.CreateStore(expr, alloc);
}

Value *NodeAssign::llvm_codegen(LLVMCompiler *compiler) {
    Value *expr = expression->llvm_codegen(compiler);

    IRBuilder<> temp_builder(
        &MAIN_FUNC->getEntryBlock(),
        MAIN_FUNC->getEntryBlock().begin()
    );
    AllocaInst* alloc;
    if(data_type == 1)
        alloc = temp_builder.CreateAlloca(compiler->builder.getInt16Ty(), 0, identifier);
    else if(data_type == 2)
        alloc = temp_builder.CreateAlloca(compiler->builder.getInt32Ty(), 0, identifier);
    else
        alloc = temp_builder.CreateAlloca(compiler->builder.getInt64Ty(), 0, identifier);

    std::string type_str1;
    raw_string_ostream rso(type_str1);
    expr->getType()->print(rso);
    if(type_str1 == "i16" && data_type == 2)
        expr = compiler->builder.CreateIntCast(expr, compiler->builder.getInt32Ty(), 1, "short to int");
    else if(type_str1 == "i16" && data_type == 3)
        expr = compiler->builder.CreateIntCast(expr, compiler->builder.getInt64Ty(), 1, "short to long");
    else if(type_str1 == "i32" && data_type == 3)
        expr = compiler->builder.CreateIntCast(expr, compiler->builder.getInt64Ty(), 1, "int to long");
    else if((type_str1 == "i16" && data_type > 1) || (type_str1 == "i32" && data_type > 2))
        yyerror("Type Mismatch");
    

    compiler->locals[identifier] = alloc;

    return compiler->builder.CreateStore(expr, alloc);
}

Value *NodeIdent::llvm_codegen(LLVMCompiler *compiler) {
    AllocaInst *alloc = compiler->locals[identifier];

    // if your LLVM_MAJOR_VERSION >= 14
    if(data_type == 1)
        return compiler->builder.CreateLoad(compiler->builder.getInt16Ty(), alloc, identifier);
    else if(data_type == 2)
        return compiler->builder.CreateLoad(compiler->builder.getInt32Ty(), alloc, identifier);
    return compiler->builder.CreateLoad(compiler->builder.getInt64Ty(), alloc, identifier);
}

#undef MAIN_FUNC