
#include "dump_tree.hpp"
#include "interpreter.hpp"
#include "lexer.hpp"
#include "parser.hpp"

int main(void) {
    // paracl::lexer::SourceBuffer source = paracl::lexer::SourceBuffer::FromFile("input.txt");
    // paracl::lexer::Lexer lexer(source);
    // std::vector<paracl::lexer::Token> tokens = lexer.TokenizeAll();

    paracl::parser::Parser parser("input.txt");
    paracl::NodePtr root = parser.Parse();

    if (!root) {
        std::cerr << "Error\n";
        return 1;
    }

    paracl::DumpTreeToImage(root.get(), "ast.dot", "ast.png");

    paracl::interpreter::Interpreter interpreter;
    if (!interpreter.RunInterpreter(*root)) {
        return 1;
    }

    return 0;
}
