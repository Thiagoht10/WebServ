#ifndef ROUTE_PARSER_HPP
#define ROUTE_PARSER_HPP

#include "ServerConfig.hpp"
#include "TokenStream.hpp"

namespace webserv
{
    class RouteParser
    {
    private:
        bool    _directListeningDefined;
        bool    _uploadEnableDefined;

        void    path(TokenStream& input, RouteConfig& route);
        void    root(TokenStream& input, RouteConfig& route);
        void    allowedMethods(TokenStream& input, RouteConfig& route);
        void    directoryListing(TokenStream& input, RouteConfig& route);
        void    index(TokenStream& input, RouteConfig& route);
        void    uploadEnabled(TokenStream& input, RouteConfig& route);
        void    uploadDirectory(TokenStream& input, RouteConfig& route);
        void    cgiInterpreter(TokenStream& input, RouteConfig& route);
        void    returnParser(TokenStream& input, RouteConfig& route);

        void    verifyUpload(TokenStream& input, const RouteConfig& route);
        void    verifyRoot(TokenStream& input, const RouteConfig& route);
        void    verifyAllowedMethods(RouteConfig& route);
        void    verifyCombination(TokenStream& input, const RouteConfig& route);
        
    public:
        RouteParser();
        ~RouteParser();
        RouteConfig parse(TokenStream& input);
    };
    
}

#endif