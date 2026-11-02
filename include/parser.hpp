#pragma once
#include <string>
#include <vector>

namespace netx {
inline std::vector<std::string> parse_command(const std::string& line) {
    std::vector<std::string> out;
    std::string token;
    char quote = 0;
    bool escaped = false, started = false;
    for (char c : line) {
        if (escaped) { token.push_back(c); escaped = false; started = true; continue; }
        if (c == '\\' && quote != '\'') { escaped = true; started = true; continue; }
        if ((c == '"' || c == '\'') && (!quote || quote == c)) { quote = quote ? 0 : c; started = true; continue; }
        if (!quote && (c == ' ' || c == '\t')) {
            if (started) { out.push_back(token); token.clear(); started = false; }
            continue;
        }
        token.push_back(c); started = true;
    }
    if (escaped) token.push_back('\\');
    if (started) out.push_back(token);
    return out;
}
}
