#include "brainfxxck/parser.hpp"
#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/parse_tree.hpp>
#include <fstream>
#include <sstream>
#include <stack>

namespace brainfxxck {

namespace pegtl = tao::pegtl;

namespace grammar {

    struct increment : pegtl::one<'+'> {};
    struct decrement : pegtl::one<'-'> {};
    struct move_right : pegtl::one<'>'> {};
    struct move_left : pegtl::one<'<'> {};
    struct output : pegtl::one<'.'> {};
    struct input : pegtl::one<','> {};
    struct loop_start : pegtl::one<'['> {};
    struct loop_end : pegtl::one<']'> {};
    
    struct instruction : pegtl::sor<
        increment,
        decrement,
        move_right,
        move_left,
        output,
        input,
        loop_start,
        loop_end
    > {};
    
    struct ignored : pegtl::not_one<'+', '-', '>', '<', '.', ',', '[', ']'> {};
    
    struct grammar : pegtl::star<pegtl::sor<instruction, ignored>> {};
    
}

struct ParserState {
    std::stack<InstructionList> instruction_stack;
    
    ParserState() {
        instruction_stack.push(InstructionList{});
    }
    
    void addInstruction(InstructionPtr instr) {
        instruction_stack.top().push_back(std::move(instr));
    }
    
    void beginLoop() {
        instruction_stack.push(InstructionList{});
    }
    
    void endLoop() {
        if (instruction_stack.size() <= 1) {
            throw ParseError("Unmatched ']' - loop end without loop start");
        }
        
        InstructionList loop_body = std::move(instruction_stack.top());
        instruction_stack.pop();
        
        auto loop = makeInstruction<Loop>(std::move(loop_body));
        instruction_stack.top().push_back(std::move(loop));
    }
    
    InstructionList finish() {
        if (instruction_stack.size() != 1) {
            throw ParseError("Unmatched '[' - loop start without loop end");
        }
        
        return std::move(instruction_stack.top());
    }
};

template<typename Rule>
struct action {};

template<>
struct action<grammar::increment> {
    template<typename ActionInput>
    static void apply(const ActionInput&, ParserState& state) {
        state.addInstruction(makeInstruction<Add>(1));
    }
};

template<>
struct action<grammar::decrement> {
    template<typename ActionInput>
    static void apply(const ActionInput&, ParserState& state) {
        state.addInstruction(makeInstruction<Add>(-1));
    }
};

template<>
struct action<grammar::move_right> {
    template<typename ActionInput>
    static void apply(const ActionInput&, ParserState& state) {
        state.addInstruction(makeInstruction<Move>(1));
    }
};

template<>
struct action<grammar::move_left> {
    template<typename ActionInput>
    static void apply(const ActionInput&, ParserState& state) {
        state.addInstruction(makeInstruction<Move>(-1));
    }
};

template<>
struct action<grammar::output> {
    template<typename ActionInput>
    static void apply(const ActionInput&, ParserState& state) {
        state.addInstruction(makeInstruction<Output>());
    }
};

template<>
struct action<grammar::input> {
    template<typename ActionInput>
    static void apply(const ActionInput&, ParserState& state) {
        state.addInstruction(makeInstruction<Input>());
    }
};

template<>
struct action<grammar::loop_start> {
    template<typename ActionInput>
    static void apply(const ActionInput&, ParserState& state) {
        state.beginLoop();
    }
};

template<>
struct action<grammar::loop_end> {
    template<typename ActionInput>
    static void apply(const ActionInput&, ParserState& state) {
        state.endLoop();
    }
};

Program Parser::parse(const std::string& source, bool optimize) {
    ParserState state;
    
    try {
        pegtl::memory_input input(source, "brainfuck-source");
        if (!pegtl::parse<grammar::grammar, action>(input, state)) {
            throw ParseError("Failed to parse brainfuck source");
        }
    } catch (const pegtl::parse_error& e) {
        throw ParseError(std::string("Parse error: ") + e.what());
    }
    
    InstructionList instructions = state.finish();
    
    if (optimize) {
        instructions = ASTOptimizer::optimize(std::move(instructions));
    }
    
    return Program(std::move(instructions));
}

Program Parser::parseFile(const std::string& filename, bool optimize) {
    std::ifstream file(filename);
    if (!file) {
        throw ParseError("Cannot open file: " + filename);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    return parse(buffer.str(), optimize);
}

}

