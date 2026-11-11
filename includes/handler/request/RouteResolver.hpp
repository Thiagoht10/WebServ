#ifndef ROUTE_RESOLVER_HPP
#define ROUTE_RESOLVER_HPP

#include "ServerConfig.hpp"
#include "ModuleInterfaces.hpp"

#include <string>

namespace webserv
{
    struct ResolvedRoute
    {
        std::string path;
        std::string query;
        std::string phisicalPath;
        const RouteConfig* route;
        int httpStatus;

        ResolvedRoute()
            : route(NULL), httpStatus(0) {}
    };
    
    class RouteResolver
    {
    private:
        struct  Impl;
        Impl*   _impl;

        void    extractPathQuery(const HttpRequest& request);
        ResolvedRoute   selectRoute(const ServerConfig& config);
        bool    verifyAddressSegments(const std::string& path);

    public:
        RouteResolver();
        ~RouteResolver();

        ResolvedRoute   resolve(const HttpRequest& request,
            const ServerConfig& config);
        
        HandlerResult   redirect(const ResolvedRoute& route, 
                const HttpRequest& request);

        bool    verifyAllowedMethods(const ResolvedRoute& route,
            const HttpRequest& request);

        std::string getAllowedMethods(const ResolvedRoute& route);

        void resolvePhisicalPath(ResolvedRoute& route);
    };
    
}

#endif