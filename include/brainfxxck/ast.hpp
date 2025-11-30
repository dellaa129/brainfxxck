#pragma once

#include <memory>
#include <vector>

namespace brainfxxck {

class Instruction;
using InstructionPtr = std::unique_ptr<Instruction>;
using InstructionList = std::vector<InstructionPtr>;

class Instruction {
public:
    virtual ~Instruction() = default;
    
    virtual void accept(class InstructionVisitor& visitor) = 0;
    virtual void accept(class ConstInstructionVisitor& visitor) const = 0;
};

class Add : public Instruction {
public:
    explicit Add(int value) : value_(value) {}
    
    int getValue() const { return value_; }
    
    void accept(InstructionVisitor& visitor) override;
    void accept(ConstInstructionVisitor& visitor) const override;
    
private:
    int value_;
};

class Move : public Instruction {
public:
    explicit Move(int offset) : offset_(offset) {}
    
    int getOffset() const { return offset_; }
    
    void accept(InstructionVisitor& visitor) override;
    void accept(ConstInstructionVisitor& visitor) const override;
    
private:
    int offset_;
};

class Input : public Instruction {
public:
    Input() = default;
    
    void accept(InstructionVisitor& visitor) override;
    void accept(ConstInstructionVisitor& visitor) const override;
};

class Output : public Instruction {
public:
    Output() = default;
    
    void accept(InstructionVisitor& visitor) override;
    void accept(ConstInstructionVisitor& visitor) const override;
};

class Loop : public Instruction {
public:
    explicit Loop(InstructionList body) : body_(std::move(body)) {}
    
    const InstructionList& getBody() const { return body_; }
    InstructionList& getBody() { return body_; }
    
    void accept(InstructionVisitor& visitor) override;
    void accept(ConstInstructionVisitor& visitor) const override;
    
private:
    InstructionList body_;
};

class Set : public Instruction {
public:
    explicit Set(int value) : value_(value) {}
    
    int getValue() const { return value_; }
    
    void accept(InstructionVisitor& visitor) override;
    void accept(ConstInstructionVisitor& visitor) const override;
    
private:
    int value_;
};

class InstructionVisitor {
public:
    virtual ~InstructionVisitor() = default;
    
    virtual void visit(Add& instruction) = 0;
    virtual void visit(Move& instruction) = 0;
    virtual void visit(Input& instruction) = 0;
    virtual void visit(Output& instruction) = 0;
    virtual void visit(Loop& instruction) = 0;
    virtual void visit(Set& instruction) = 0;
};

class ConstInstructionVisitor {
public:
    virtual ~ConstInstructionVisitor() = default;
    
    virtual void visit(const Add& instruction) = 0;
    virtual void visit(const Move& instruction) = 0;
    virtual void visit(const Input& instruction) = 0;
    virtual void visit(const Output& instruction) = 0;
    virtual void visit(const Loop& instruction) = 0;
    virtual void visit(const Set& instruction) = 0;
};

class Program {
public:
    explicit Program(InstructionList instructions) 
        : instructions_(std::move(instructions)) {}
    
    const InstructionList& getInstructions() const { return instructions_; }
    InstructionList& getInstructions() { return instructions_; }
    
private:
    InstructionList instructions_;
};

template<typename T, typename... Args>
InstructionPtr makeInstruction(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

class ASTOptimizer {
public:
    static InstructionList optimize(InstructionList instructions);
    
private:
    static bool isSimpleClear(const Loop& loop);
    static InstructionList optimizeLoop(Loop& loop);
};

}

