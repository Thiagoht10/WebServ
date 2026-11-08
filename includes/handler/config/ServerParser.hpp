#ifndef SERVER_PARSER_HPP
#define SERVER_PARSER_HPP

#include <map>

#include "ServerConfig.hpp"
#include "TokenStream.hpp"

namespace webserv
{
    class TokenStream;

    class ServerParser
    {
    private:
        bool    _maxBodyDefined;

        bool    onlyDigit(const std::string& value) const;

        void    parseListen(TokenStream& input, ServerConfig& server);
        void    maxBody(TokenStream& input, ServerConfig& server);
        void    errorPage(TokenStream& input, ServerConfig& server);

        void    verifyListeners(const TokenStream& input, const ServerConfig& server);
        void    verifyRoutes(const TokenStream& input, const ServerConfig& Server);

    public:
        ServerParser();
        ~ServerParser();
        ServerConfig parse(TokenStream& input);
    };
    
}

#endif
