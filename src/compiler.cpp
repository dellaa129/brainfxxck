#include "brainfxxck/compiler.hpp"
#include "brainfxxck/codegen.hpp"
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <cstdlib>
#include <unistd.h>

namespace brainfxxck {

Compiler::Compiler(unsigned opt_level, const std::string& target_triple)
    : opt_level_(opt_level),
      target_triple_(target_triple),
      target_machine_(nullptr) {
    initializeTarget();
}

Compiler::~Compiler() {
    delete target_machine_;
}

void Compiler::initializeTarget() {
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();
    
    if (target_triple_.empty()) {
        target_triple_ = llvm::sys::getDefaultTargetTriple();
    }
    
    std::string error;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(target_triple_, error);
    
    if (!target) {
        throw std::runtime_error("Failed to lookup target: " + error);
    }
    
    llvm::TargetOptions options;
    llvm::Reloc::Model reloc_model = llvm::Reloc::PIC_;
    
    target_machine_ = target->createTargetMachine(
        target_triple_,
        "generic",
        "",
        options,
        reloc_model,
        std::nullopt,
        llvm::CodeGenOptLevel::Default
    );
    
    if (!target_machine_) {
        throw std::runtime_error("Failed to create target machine");
    }
}

void Compiler::optimizeModule(llvm::Module& module) {
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

void Compiler::emitToFile(llvm::Module& module,
                         const std::string& output_file,
                         OutputFormat format) {
    std::error_code error_code;
    llvm::raw_fd_ostream dest(output_file, error_code, llvm::sys::fs::OF_None);
    
    if (error_code) {
        throw std::runtime_error("Could not open file: " + error_code.message());
    }
    
    switch (format) {
        case OutputFormat::LLVMIR: {
            module.print(dest, nullptr);
            break;
        }
        
        case OutputFormat::LLVMBitcode: {
            llvm::WriteBitcodeToFile(module, dest);
            break;
        }
        
        case OutputFormat::AssemblyFile:
        case OutputFormat::ObjectFile: {
            llvm::legacy::PassManager pass;
            auto file_type = (format == OutputFormat::AssemblyFile)
                ? llvm::CodeGenFileType::AssemblyFile
                : llvm::CodeGenFileType::ObjectFile;
            
            if (target_machine_->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
                throw std::runtime_error("Target machine can't emit file of this type");
            }
            
            pass.run(module);
            break;
        }
    }
    
    dest.flush();
}

void Compiler::compile(const Program& program,
                      const std::string& output_file,
                      OutputFormat format) {
    llvm::LLVMContext context;
    CodeGenerator codegen(context, "brainfuck", 30000, false);
    
    codegen.generate(program);
    
    auto module = codegen.takeModule();
    
    module->setTargetTriple(target_triple_);
    module->setDataLayout(target_machine_->createDataLayout());
    
    optimizeModule(*module);
    
    emitToFile(*module, output_file, format);
}

void Compiler::compileToExecutable(const Program& program,
                                  const std::string& output_file) {
    llvm::LLVMContext context;
    CodeGenerator codegen(context, "brainfuck", 30000, true);
    codegen.generate(program);
    auto module = codegen.takeModule();
    
    module->setTargetTriple(target_triple_);
    module->setDataLayout(target_machine_->createDataLayout());
    
    optimizeModule(*module);
    
    char temp_template[] = "/tmp/brainfxxck_XXXXXX.o";
    int fd = mkstemps(temp_template, 2);
    if (fd == -1) {
        throw std::runtime_error("Failed to create temporary file");
    }
    close(fd);
    std::string obj_file = temp_template;
    
    emitToFile(*module, obj_file, OutputFormat::ObjectFile);
    
    std::string cc = "cc";
    if (std::getenv("CC")) {
        cc = std::getenv("CC");
    }
    
    std::string link_cmd = cc + " " + obj_file + " -o " + output_file;
    
    int result = std::system(link_cmd.c_str());
    
    std::remove(obj_file.c_str());
    
    if (result != 0) {
        throw std::runtime_error("Linking failed");
    }
}

std::string Compiler::getTargetTriple() const {
    return target_triple_;
}

}

