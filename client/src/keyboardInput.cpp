#include "../include/keyboardInput.h"
#include <iostream>
#include <sstream>

// Reads a full line of input from the user
std::string KeyboardInput::readLine() {
    std::string input;
    std::getline(std::cin, input);
    return input;
}

// Splits a string and stores the tokens in an existing vector
void KeyboardInput::split_str(const std::string& str, char delimiter, std::vector<std::string>& result) {
    std::stringstream ss(str);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) { // Ignore empty tokens
            result.push_back(token);
        }
    }
}
