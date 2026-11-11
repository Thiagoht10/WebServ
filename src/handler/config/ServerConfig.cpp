#include "ServerConfig.hpp"
#include "ConfigLexer.hpp"
#include "TokenStream.hpp"
#include "ConfigParser.hpp"
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <iostream>
#include <sys/stat.h>

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
        struct stat info;

        if (stat(path.c_str(), &info) == -1)
            throw std::runtime_error("unable to inspect configuration file '" + path + "'");

        if (!S_ISREG(info.st_mode))
            throw std::runtime_error("configuration path is not a regular file: '" + path + "'");
        
        std::ifstream file(path.c_str());

        if (!file.is_open())
            throw std::runtime_error("unable to open configuration file '" + path + "'");

        std::ostringstream buffer;
        buffer << file.rdbuf();

        if (buffer.bad())
            throw std::runtime_error("unable to read configuration file '" + path + "'");

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

        ConfigParser parser;

        return parser.parse(input);
    }
}
