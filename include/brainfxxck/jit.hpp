#pragma once

#include "brainfxxck/ast.hpp"
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>
#include <string>

namespace brainfxxck {

class JITEngine {
public:
    explicit JITEngine(unsigned opt_level = 2);
    
    void execute(const Program& program);
    
    void addModule(std::unique_ptr<llvm::Module> module);
    
    llvm::Expected<llvm::orc::ExecutorAddr> lookup(const std::string& name);
    
    unsigned getOptLevel() const { return opt_level_; }
    
private:
    std::unique_ptr<llvm::orc::LLJIT> jit_;
    unsigned opt_level_;
    
    void optimizeModule(llvm::Module& module);
    void initializeJIT();
};

}

