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
        size_t  numMaxBodyDefined;

        void    throwExpect(const Token& token, const std::string& msg);
        void    parseListen(TokenStream& input, ServerConfig& server);
        void    maxBody(TokenStream& input, ServerConfig& server);
        void    errorPage(TokenStream& input, ServerConfig& server);

    public:
        ServerParser();
        ~ServerParser();
        ServerConfig parse(TokenStream& input);
    };
    
}

#endif