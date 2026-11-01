#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include "TokenStream.hpp"
#include "ServerConfig.hpp"

namespace webserv
{
    class TokenStream;

    class ConfigParser
    {
    public:
        std::vector<ServerConfig> parse(TokenStream& input);
    };
    
}

#endif