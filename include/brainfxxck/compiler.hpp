#pragma once

#include "brainfxxck/ast.hpp"
#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>
#include <string>

namespace brainfxxck {

class Compiler {
public:
    enum class OutputFormat {
        ObjectFile,
        AssemblyFile,
        LLVMBitcode,
        LLVMIR
    };
    
    explicit Compiler(unsigned opt_level = 2, 
                     const std::string& target_triple = "");
    
    ~Compiler();
    
    void compile(const Program& program, 
                const std::string& output_file,
                OutputFormat format = OutputFormat::ObjectFile);
    
    void compileToExecutable(const Program& program,
                            const std::string& output_file);
    
    unsigned getOptLevel() const { return opt_level_; }
    
    std::string getTargetTriple() const;
    
private:
    unsigned opt_level_;
    // cppcheck-suppress unusedStructMember
    std::string target_triple_;
    // cppcheck-suppress unusedStructMember
    llvm::TargetMachine* target_machine_;
    
    void initializeTarget();
    void optimizeModule(llvm::Module& module);
    void emitToFile(llvm::Module& module, 
                   const std::string& output_file,
                   OutputFormat format);
};

}

