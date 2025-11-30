#include "brainfxxck/ast.hpp"

namespace brainfxxck {

void Add::accept(InstructionVisitor& visitor) {
    visitor.visit(*this);
}

void Add::accept(ConstInstructionVisitor& visitor) const {
    visitor.visit(*this);
}

void Move::accept(InstructionVisitor& visitor) {
    visitor.visit(*this);
}

void Move::accept(ConstInstructionVisitor& visitor) const {
    visitor.visit(*this);
}

void Input::accept(InstructionVisitor& visitor) {
    visitor.visit(*this);
}

void Input::accept(ConstInstructionVisitor& visitor) const {
    visitor.visit(*this);
}

void Output::accept(InstructionVisitor& visitor) {
    visitor.visit(*this);
}

void Output::accept(ConstInstructionVisitor& visitor) const {
    visitor.visit(*this);
}

void Loop::accept(InstructionVisitor& visitor) {
    visitor.visit(*this);
}

void Loop::accept(ConstInstructionVisitor& visitor) const {
    visitor.visit(*this);
}

void Set::accept(InstructionVisitor& visitor) {
    visitor.visit(*this);
}

void Set::accept(ConstInstructionVisitor& visitor) const {
    visitor.visit(*this);
}

bool ASTOptimizer::isSimpleClear(const Loop& loop) {
    const auto& body = loop.getBody();
    
    if (body.size() == 1) {
        if (auto* add = dynamic_cast<Add*>(body[0].get())) {
            return add->getValue() == -1 || add->getValue() == 1;
        }
    }
    
    return false;
}

InstructionList ASTOptimizer::optimizeLoop(Loop& loop) {
    InstructionList result;
    
    if (isSimpleClear(loop)) {
        result.push_back(makeInstruction<Set>(0));
        return result;
    }
    
    loop.getBody() = optimize(std::move(loop.getBody()));
    result.push_back(makeInstruction<Loop>(std::move(loop.getBody())));
    
    return result;
}

InstructionList ASTOptimizer::optimize(InstructionList instructions) {
    InstructionList result;
    
    size_t i = 0;
    while (i < instructions.size()) {
        auto& instr = instructions[i];
        
        if (auto* add = dynamic_cast<Add*>(instr.get())) {
            int total = add->getValue();
            size_t j = i + 1;
            
            while (j < instructions.size()) {
                if (const auto* next_add = dynamic_cast<const Add*>(instructions[j].get())) {
                    total += next_add->getValue();
                    j++;
                } else {
                    break;
                }
            }
            
            if (total != 0) {
                result.push_back(makeInstruction<Add>(total));
            }
            i = j;
        }
        else if (auto* move = dynamic_cast<Move*>(instr.get())) {
            int total = move->getOffset();
            size_t j = i + 1;
            
            while (j < instructions.size()) {
                if (const auto* next_move = dynamic_cast<const Move*>(instructions[j].get())) {
                    total += next_move->getOffset();
                    j++;
                } else {
                    break;
                }
            }
            
            if (total != 0) {
                result.push_back(makeInstruction<Move>(total));
            }
            i = j;
        }
        else if (auto* loop = dynamic_cast<Loop*>(instr.get())) {
            auto optimized = optimizeLoop(*loop);
            result.reserve(result.size() + optimized.size());
            std::move(optimized.begin(), optimized.end(), std::back_inserter(result));
            i++;
        }
        else {
            result.push_back(std::move(instr));
            i++;
        }
    }
    
    return result;
}

}

