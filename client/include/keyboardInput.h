#pragma once

#include <string>
#include <vector>

class KeyboardInput {
public:
    static std::string readLine(); // Reads a full line of user input
    static void split_str(const std::string& str, char delimiter, std::vector<std::string>& result); // Splits a string and stores tokens in an existing vector
};
