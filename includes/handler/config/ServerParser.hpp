#ifndef SERVER_PARSER_HPP
#define SERVER_PARSER_HPP

#include "ServerConfig.hpp"
#include "TokenStream.hpp"

namespace webserv
{
    class TokenStream;

    class ServerParser
    {
    private:
        void    parseListen(TokenStream& input, ServerConfig& server);
        void    maxBody(TokenStream& input, ServerConfig& server);
        void    errorPage(TokenStream& input, ServerConfig& server);
    public:
        ServerConfig parse(TokenStream& input);
    };
    
}

#endif