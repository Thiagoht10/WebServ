#include "ServerConfig.hpp"
#include "ConfigLexer.hpp"
#include "TokenStream.hpp"
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <iostream>

namespace
{
    // Temporario: visualizar a saida do lexer enquanto o parser esta em desenvolvimento.
    /* void printTokens(const std::vector<webserv::Token>& tokens)
    {
        for (std::size_t i = 0; i < tokens.size(); ++i)
            std::cout << "linha " << tokens[i].line
                      << ": [" << tokens[i].value << "]\n";
    } */

    std::string readFile(const std::string& path)
    {
        std::ifstream file(path.c_str());

        if (!file.is_open())
            throw std::runtime_error("cannot open config file");

        std::ostringstream buffer;
        buffer << file.rdbuf();

        if (buffer.bad())
            throw std::runtime_error("error reading config file");

        return buffer.str();
    }
}

namespace webserv
{
    std::vector<ServerConfig> ConfigLoader::load(const std::string& path) const
    {
        std::string content = readFile(path);

        ConfigLexer lexer;
        std::vector<Token> tokens = lexer.tokenizer(content);

        TokenStream input(tokens);
        //printTokens(tokens);
        // Temporario: o parser ainda nao produz ServerConfig.

        return std::vector<ServerConfig>();
    }
}
