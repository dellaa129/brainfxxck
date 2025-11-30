#pragma once

#include "brainfxxck/ast.hpp"
#include <string>
#include <stdexcept>

namespace brainfxxck {

class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& message)
        : std::runtime_error(message) {}
};

class Parser {
public:
    static Program parse(const std::string& source, bool optimize = true);
    
    static Program parseFile(const std::string& filename, bool optimize = true);
    
private:
    Parser() = default;
};

}

