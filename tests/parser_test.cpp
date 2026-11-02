#include "parser.hpp"
#include <cassert>
int main() {
    using netx::parse_command;
    assert((parse_command("help") == std::vector<std::string>{"help"}));
    assert((parse_command("wmap scan \"https://example.test/a b\"") == std::vector<std::string>{"wmap","scan","https://example.test/a b"}));
    assert((parse_command("netmap ports 127.0.0.1 --range 1-10") == std::vector<std::string>{"netmap","ports","127.0.0.1","--range","1-10"}));
    assert((parse_command("  a 'b c'  ") == std::vector<std::string>{"a","b c"}));
}
