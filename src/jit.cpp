#include "brainfxxck/jit.hpp"
#include "brainfxxck/codegen.hpp"
#include <llvm/Support/TargetSelect.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/IR/Verifier.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>

namespace brainfxxck {

JITEngine::JITEngine(unsigned opt_level)
    : opt_level_(opt_level) {
    initializeJIT();
}

void JITEngine::initializeJIT() {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    
    auto jit_builder = llvm::orc::LLJITBuilder();
    auto jit_expected = jit_builder.create();
    
    if (!jit_expected) {
        llvm::errs() << "Failed to create JIT: " << llvm::toString(jit_expected.takeError()) << "\n";
        throw std::runtime_error("Failed to create JIT engine");
    }
    
    jit_ = std::move(*jit_expected);
    
    auto& main_jd = jit_->getMainJITDylib();
    auto generator = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
        jit_->getDataLayout().getGlobalPrefix()
    );
    
    if (!generator) {
        llvm::errs() << "Failed to create symbol generator: " 
                     << llvm::toString(generator.takeError()) << "\n";
        throw std::runtime_error("Failed to create symbol generator");
    }
    
    main_jd.addGenerator(std::move(*generator));
}

void JITEngine::optimizeModule(llvm::Module& module) {
    if (opt_level_ == 0) {
        return;
    }
    
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;
    
    llvm::PassBuilder PB;
    
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
    
    llvm::ModulePassManager MPM;
    llvm::OptimizationLevel opt_level;
    
    switch (opt_level_) {
        case 1:
            opt_level = llvm::OptimizationLevel::O1;
            break;
        case 2:
            opt_level = llvm::OptimizationLevel::O2;
            break;
        case 3:
            opt_level = llvm::OptimizationLevel::O3;
            break;
        default:
            opt_level = llvm::OptimizationLevel::O0;
            break;
    }
    
    MPM = PB.buildPerModuleDefaultPipeline(opt_level);
    
    MPM.run(module, MAM);
}

void JITEngine::addModule(std::unique_ptr<llvm::Module> module) {
    optimizeModule(*module);
    
    auto context = std::make_unique<llvm::LLVMContext>();
    auto tsm = llvm::orc::ThreadSafeModule(std::move(module), std::move(context));
    
    auto err = jit_->addIRModule(std::move(tsm));
    if (err) {
        llvm::errs() << "Failed to add module: " << llvm::toString(std::move(err)) << "\n";
        throw std::runtime_error("Failed to add module to JIT");
    }
}

llvm::Expected<llvm::orc::ExecutorAddr> JITEngine::lookup(const std::string& name) {
    return jit_->lookup(name);
}

void JITEngine::execute(const Program& program) {
    auto context = std::make_unique<llvm::LLVMContext>();
    CodeGenerator codegen(*context);
    
    codegen.generate(program);
    
    auto module = codegen.takeModule();
    
    optimizeModule(*module);
    
    auto tsm = llvm::orc::ThreadSafeModule(std::move(module), std::move(context));
    
    auto err = jit_->addIRModule(std::move(tsm));
    if (err) {
        llvm::errs() << "Failed to add module: " << llvm::toString(std::move(err)) << "\n";
        throw std::runtime_error("Failed to add module to JIT");
    }
    
    auto main_symbol = jit_->lookup("brainfuck_main");
    if (!main_symbol) {
        llvm::errs() << "Failed to find brainfuck_main: " 
                     << llvm::toString(main_symbol.takeError()) << "\n";
        throw std::runtime_error("Failed to find main function");
    }
    
    auto main_addr = main_symbol->getValue();
    auto* main_func = reinterpret_cast<void(*)()>(main_addr);
    
    main_func();
}

}

