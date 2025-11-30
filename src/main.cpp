#include "brainfxxck/parser.hpp"
#include "brainfxxck/jit.hpp"
#include "brainfxxck/compiler.hpp"
#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>

namespace {

void printUsage(const char* program_name) {
    std::cout << "brainfxxck\n";
    std::cout << "Modern C++ brainfuck interpreter with LLVM 18\n";
    std::cout << "Copyright (c) 2025. Licensed under MIT License\n";
    std::cerr << "\nUsage: " << program_name << " [OPTIONS] <file>\n";
    std::cerr << "\nOptions:\n";
    std::cerr << "  --jit              Run using JIT compilation (default)\n";
    std::cerr << "  --compile          Compile to native binary\n";
    std::cerr << "  -o <output>        Output file for compilation\n";
    std::cerr << "  -O<level>          Optimization level (0-3, default: 2)\n";
    std::cerr << "  --emit-llvm        Emit LLVM IR instead of binary\n";
    std::cerr << "  --emit-asm         Emit assembly instead of binary\n";
    std::cerr << "  --emit-bc          Emit LLVM bitcode instead of binary\n";
    std::cerr << "  --no-optimize      Disable AST optimizations\n";
    std::cerr << "  --help             Show this help message\n";
    std::cerr << "\nExamples:\n";
    std::cerr << "  " << program_name << " program.bf                  # Run with JIT\n";
    std::cerr << "  " << program_name << " --compile -o prog program.bf # Compile to executable\n";
    std::cerr << "  " << program_name << " -O3 program.bf              # Run with max optimization\n";
}

struct Options {
    std::string input_file;
    std::string output_file;
    bool use_jit = true;
    unsigned opt_level = 2;
    bool ast_optimize = true;
    brainfxxck::Compiler::OutputFormat output_format = brainfxxck::Compiler::OutputFormat::ObjectFile;
    bool emit_executable = true;
};

Options parseArguments(int argc, char* argv[]) {
    Options opts;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            std::exit(0);
        }
        else if (arg == "--jit") {
            opts.use_jit = true;
        }
        else if (arg == "--compile") {
            opts.use_jit = false;
        }
        else if (arg == "-o") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -o requires an argument\n";
                std::exit(1);
            }
            opts.output_file = argv[++i];
        }
        else if (arg.substr(0, 2) == "-O") {
            if (arg.length() == 3 && arg[2] >= '0' && arg[2] <= '3') {
                opts.opt_level = arg[2] - '0';
            } else {
                std::cerr << "Error: Invalid optimization level: " << arg << "\n";
                std::exit(1);
            }
        }
        else if (arg == "--emit-llvm") {
            opts.use_jit = false;
            opts.output_format = brainfxxck::Compiler::OutputFormat::LLVMIR;
            opts.emit_executable = false;
        }
        else if (arg == "--emit-asm") {
            opts.use_jit = false;
            opts.output_format = brainfxxck::Compiler::OutputFormat::AssemblyFile;
            opts.emit_executable = false;
        }
        else if (arg == "--emit-bc") {
            opts.use_jit = false;
            opts.output_format = brainfxxck::Compiler::OutputFormat::LLVMBitcode;
            opts.emit_executable = false;
        }
        else if (arg == "--no-optimize") {
            opts.ast_optimize = false;
        }
        else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            std::exit(1);
        }
        else {
            if (opts.input_file.empty()) {
                opts.input_file = arg;
            } else {
                std::cerr << "Error: Multiple input files specified\n";
                std::exit(1);
            }
        }
    }
    
    if (opts.input_file.empty()) {
        std::cerr << "Error: No input file specified\n";
        printUsage(argv[0]);
        std::exit(1);
    }
    
    if (!opts.use_jit && opts.output_file.empty()) {
        if (opts.emit_executable) {
            opts.output_file = "a.out";
        } else {
            size_t dot_pos = opts.input_file.find_last_of('.');
            std::string base = (dot_pos != std::string::npos) 
                ? opts.input_file.substr(0, dot_pos) 
                : opts.input_file;
            
            switch (opts.output_format) {
                case brainfxxck::Compiler::OutputFormat::LLVMIR:
                    opts.output_file = base + ".ll";
                    break;
                case brainfxxck::Compiler::OutputFormat::AssemblyFile:
                    opts.output_file = base + ".s";
                    break;
                case brainfxxck::Compiler::OutputFormat::LLVMBitcode:
                    opts.output_file = base + ".bc";
                    break;
                case brainfxxck::Compiler::OutputFormat::ObjectFile:
                    opts.output_file = base + ".o";
                    break;
            }
        }
    }
    
    return opts;
}

}

int main(int argc, char* argv[]) {
    try {
        Options opts = parseArguments(argc, argv);
        
        brainfxxck::Program program = brainfxxck::Parser::parseFile(
            opts.input_file,
            opts.ast_optimize
        );
        
        if (opts.use_jit) {
            brainfxxck::JITEngine jit(opts.opt_level);
            jit.execute(program);
        } else {
            brainfxxck::Compiler compiler(opts.opt_level);
            
            if (opts.emit_executable) {
                compiler.compileToExecutable(program, opts.output_file);
                std::cerr << "Compiled to: " << opts.output_file << "\n";
            } else {
                compiler.compile(program, opts.output_file, opts.output_format);
                std::cerr << "Generated: " << opts.output_file << "\n";
            }
        }
        
        return 0;
        
    } catch (const brainfxxck::ParseError& e) {
        std::cerr << "Parse error: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

