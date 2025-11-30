#include "brainfxxck/codegen.hpp"
#include <llvm/IR/Verifier.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>

namespace brainfxxck {

CodeGenerator::CodeGenerator(llvm::LLVMContext& context,
                            const std::string& module_name,
                            size_t tape_size,
                            bool create_main_wrapper)
    : context_(context),
      module_(std::make_unique<llvm::Module>(module_name, context)),
      builder_(std::make_unique<llvm::IRBuilder<>>(context)),
      main_function_(nullptr),
      putchar_function_(nullptr),
      getchar_function_(nullptr),
      tape_ptr_(nullptr),
      ptr_(nullptr),
      tape_size_(tape_size),
      create_main_wrapper_(create_main_wrapper) {
}

void CodeGenerator::createRuntimeFunctions() {
    llvm::FunctionType* putchar_type = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(context_),
        {llvm::Type::getInt32Ty(context_)},
        false
    );
    // cppcheck-suppress unusedStructMember
    putchar_function_ = llvm::Function::Create(
        putchar_type,
        llvm::Function::ExternalLinkage,
        "putchar",
        module_.get()
    );
    
    llvm::FunctionType* getchar_type = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(context_),
        {},
        false
    );
    // cppcheck-suppress unusedStructMember
    getchar_function_ = llvm::Function::Create(
        getchar_type,
        llvm::Function::ExternalLinkage,
        "getchar",
        module_.get()
    );
}

llvm::Function* CodeGenerator::getMallocFunction() {
    llvm::FunctionType* malloc_type = llvm::FunctionType::get(
        llvm::PointerType::get(llvm::Type::getInt8Ty(context_), 0),
        {llvm::Type::getInt64Ty(context_)},
        false
    );
    return llvm::Function::Create(
        malloc_type,
        llvm::Function::ExternalLinkage,
        "malloc",
        module_.get()
    );
}

llvm::Function* CodeGenerator::getFreeFunction() {
    llvm::FunctionType* free_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(context_),
        {llvm::PointerType::get(llvm::Type::getInt8Ty(context_), 0)},
        false
    );
    return llvm::Function::Create(
        free_type,
        llvm::Function::ExternalLinkage,
        "free",
        module_.get()
    );
}

void CodeGenerator::createMainFunction() {
    llvm::FunctionType* main_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(context_),
        {},
        false
    );
    main_function_ = llvm::Function::Create(
        main_type,
        llvm::Function::ExternalLinkage,
        "brainfuck_main",
        module_.get()
    );
    
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", main_function_);
    builder_->SetInsertPoint(entry);
    
    llvm::Function* malloc_fn = getMallocFunction();
    llvm::Value* tape_size_val = llvm::ConstantInt::get(
        llvm::Type::getInt64Ty(context_),
        tape_size_
    );
    // cppcheck-suppress unusedStructMember
    tape_ptr_ = builder_->CreateCall(malloc_fn, {tape_size_val}, "tape");
    
    builder_->CreateMemSet(
        tape_ptr_,
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(context_), 0),
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), tape_size_),
        llvm::MaybeAlign(1)
    );
    
    // cppcheck-suppress unusedStructMember
    ptr_ = builder_->CreateAlloca(llvm::Type::getInt32Ty(context_), nullptr, "ptr");
    builder_->CreateStore(
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 0),
        ptr_
    );
}

llvm::Value* CodeGenerator::getCurrentCell() {
    llvm::Value* idx = builder_->CreateLoad(llvm::Type::getInt32Ty(context_), ptr_, "idx");
    
    llvm::Value* idx64 = builder_->CreateZExt(idx, llvm::Type::getInt64Ty(context_), "idx64");
    llvm::Value* cell_ptr = builder_->CreateGEP(
        llvm::Type::getInt8Ty(context_),
        tape_ptr_,
        idx64,
        "cell_ptr"
    );
    
    return builder_->CreateLoad(llvm::Type::getInt8Ty(context_), cell_ptr, "cell");
}

void CodeGenerator::setCurrentCell(llvm::Value* value) {
    llvm::Value* idx = builder_->CreateLoad(llvm::Type::getInt32Ty(context_), ptr_, "idx");
    
    llvm::Value* idx64 = builder_->CreateZExt(idx, llvm::Type::getInt64Ty(context_), "idx64");
    llvm::Value* cell_ptr = builder_->CreateGEP(
        llvm::Type::getInt8Ty(context_), tape_ptr_,
        idx64,
        "cell_ptr"
    );
    
    builder_->CreateStore(value, cell_ptr);
}

void CodeGenerator::visit(const Add& instruction) {
    llvm::Value* cell = getCurrentCell();
    llvm::Value* new_value = builder_->CreateAdd(
        cell,
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(context_), instruction.getValue()),
        "add"
    );
    setCurrentCell(new_value);
}

void CodeGenerator::visit(const Move& instruction) {
    llvm::Value* idx = builder_->CreateLoad(llvm::Type::getInt32Ty(context_), ptr_, "idx");
    llvm::Value* offset_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), instruction.getOffset());
    llvm::Value* new_idx = builder_->CreateAdd(idx, offset_val, "move_add");
    
    llvm::Value* tape_size_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), static_cast<int32_t>(tape_size_));
    llvm::Value* modulo_idx = builder_->CreateSRem(new_idx, tape_size_val, "move_mod");
    llvm::Value* is_negative = builder_->CreateICmpSLT(modulo_idx, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 0), "is_neg");
    llvm::Value* wrapped_idx = builder_->CreateSelect(
        is_negative,
        builder_->CreateAdd(modulo_idx, tape_size_val, "wrap_add"),
        modulo_idx,
        "wrapped_idx"
    );
    
    builder_->CreateStore(wrapped_idx, ptr_);
}

void CodeGenerator::visit(const Input& instruction) {
    llvm::Value* ch = builder_->CreateCall(getchar_function_, {}, "input");
    
    llvm::Value* ch8 = builder_->CreateTrunc(ch, llvm::Type::getInt8Ty(context_), "ch8");
    
    setCurrentCell(ch8);
}

void CodeGenerator::visit(const Output& instruction) {
    llvm::Value* cell = getCurrentCell();
    
    llvm::Value* ch32 = builder_->CreateZExt(cell, llvm::Type::getInt32Ty(context_), "ch32");
    
    builder_->CreateCall(putchar_function_, {ch32});
}

void CodeGenerator::visit(const Loop& instruction) {
    llvm::BasicBlock* loop_cond = llvm::BasicBlock::Create(context_, "loop.cond", main_function_);
    llvm::BasicBlock* loop_body = llvm::BasicBlock::Create(context_, "loop.body", main_function_);
    llvm::BasicBlock* loop_end = llvm::BasicBlock::Create(context_, "loop.end", main_function_);
    
    builder_->CreateBr(loop_cond);
    
    builder_->SetInsertPoint(loop_cond);
    llvm::Value* cell = getCurrentCell();
    llvm::Value* cond = builder_->CreateICmpNE(
        cell,
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(context_), 0),
        "loop.cond"
    );
    builder_->CreateCondBr(cond, loop_body, loop_end);
    
    builder_->SetInsertPoint(loop_body);
    generateInstructions(instruction.getBody());
    builder_->CreateBr(loop_cond);
    
    builder_->SetInsertPoint(loop_end);
}

void CodeGenerator::visit(const Set& instruction) {
    llvm::Value* value = llvm::ConstantInt::get(
        llvm::Type::getInt8Ty(context_),
        instruction.getValue()
    );
    setCurrentCell(value);
}

void CodeGenerator::generateInstructions(const InstructionList& instructions) {
    for (const auto& instr : instructions) {
        instr->accept(*this);
    }
}

void CodeGenerator::createMainWrapper() {
    llvm::FunctionType* main_type = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(context_),
        {},
        false
    );
    llvm::Function* main_wrapper = llvm::Function::Create(
        main_type,
        llvm::Function::ExternalLinkage,
        "main",
        module_.get()
    );
    
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", main_wrapper);
    builder_->SetInsertPoint(entry);
    
    builder_->CreateCall(main_function_, {});
    
    builder_->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 0));
}

void CodeGenerator::generate(const Program& program) {
    createRuntimeFunctions();
    
    createMainFunction();
    
    generateInstructions(program.getInstructions());
    
    llvm::Function* free_fn = getFreeFunction();
    builder_->CreateCall(free_fn, {tape_ptr_});
    
    builder_->CreateRetVoid();
    
    // cppcheck-suppress unusedStructMember
    if (create_main_wrapper_) {
        createMainWrapper();
    }
    
    std::string error;
    llvm::raw_string_ostream error_stream(error);
    if (llvm::verifyModule(*module_, &error_stream)) {
        throw std::runtime_error("Module verification failed: " + error);
    }
}

std::unique_ptr<llvm::Module> CodeGenerator::takeModule() {
    return std::move(module_);
}

}

