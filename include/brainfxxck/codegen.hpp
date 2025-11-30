#pragma once

#include "brainfxxck/ast.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

namespace brainfxxck {

class CodeGenerator : public ConstInstructionVisitor {
public:
    explicit CodeGenerator(llvm::LLVMContext& context, 
                          const std::string& module_name = "brainfuck",
                          size_t tape_size = 30000,
                          bool create_main_wrapper = false);
    
    void generate(const Program& program);
    
    std::unique_ptr<llvm::Module> takeModule();
    
    llvm::Function* getMainFunction() const { return main_function_; }
    
    void visit(const Add& instruction) override;
    void visit(const Move& instruction) override;
    void visit(const Input& instruction) override;
    void visit(const Output& instruction) override;
    void visit(const Loop& instruction) override;
    void visit(const Set& instruction) override;
    
private:
    llvm::LLVMContext& context_;
    std::unique_ptr<llvm::Module> module_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;
    
    llvm::Function* main_function_;
    // cppcheck-suppress unusedStructMember
    llvm::Function* putchar_function_;
    // cppcheck-suppress unusedStructMember
    llvm::Function* getchar_function_;
    
    // cppcheck-suppress unusedStructMember
    llvm::Value* tape_ptr_;
    // cppcheck-suppress unusedStructMember
    llvm::Value* ptr_;
    // cppcheck-suppress unusedStructMember
    size_t tape_size_;
    // cppcheck-suppress unusedStructMember
    bool create_main_wrapper_;
    
    void createRuntimeFunctions();
    void createMainFunction();
    void createMainWrapper();
    llvm::Function* getMallocFunction();
    llvm::Function* getFreeFunction();
    llvm::Value* getCurrentCell();
    void setCurrentCell(llvm::Value* value);
    void generateInstructions(const InstructionList& instructions);
};

}

