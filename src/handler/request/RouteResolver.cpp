#include "RouteResolver.hpp"
#include <sstream>

namespace webserv
{
    struct RouteResolver::Impl
    {
        std::string path;
        std::string query;
    };

    RouteResolver::RouteResolver()
        : _impl(new Impl())
    {}

    RouteResolver::~RouteResolver()
    {
        delete _impl;
    }

    ResolvedRoute   RouteResolver::resolve(const HttpRequest& request,
            const ServerConfig& config)
    {
        ResolvedRoute route;

        extractPathQuery(request);
        route = selectRoute(config);

        return route;
    }

    void    RouteResolver::extractPathQuery(const HttpRequest& request)
    {
        std::string target;
        size_t      pos;

        target = request.target;

        pos = target.find('?');
        if (pos == std::string::npos)
        {
            _impl->path = target;
            _impl->query.clear();
        }
        else
        {
            _impl->path = target.substr(0, pos);
            _impl->query = target.substr(pos + 1);
        }

        return;
    }

    ResolvedRoute    RouteResolver::selectRoute(const ServerConfig& config)
    {
        ResolvedRoute route;
        route.path = _impl->path;
        route.query = _impl->query;

        size_t longest = 0;

        for (size_t i = 0; i < config.routes.size(); i++)
        {
            const std::string&  prefix = config.routes[i].path;
            const std::string&  path = _impl->path;

            bool    matches = false;

            if (prefix == "/" || path == prefix)
                matches = true;

            else if (path.size() > prefix.size() 
                && path.compare(0, prefix.size(), prefix) == 0)
            {
                if (prefix[prefix.size() - 1] == '/' || path[prefix.size()] == '/')
                    matches = true;
            }

            if (matches && prefix.size() > longest)
            {
                route.route = &config.routes[i];
                longest = prefix.size();
            }
        }

        if (route.route == NULL)
            route.httpStatus = 404;

        return route;                
    }

    HandlerResult RouteResolver::redirect(const ResolvedRoute& route, 
                const HttpRequest& request)
    {
        HandlerResult result;

        result.state = HANDLER_READY;

        result.response.statusCode = route.route->redirectStatus;
        result.response.body.clear();
        result.response.closeConnection = !request.keepAlive;

        HttpHeader header;
        header.name = "Location";
        header.value = route.route->redirectTarget;

        result.response.headers.push_back(header);

        return result;
    }

    bool    RouteResolver::verifyAllowedMethods(const ResolvedRoute& route,
            const HttpRequest& request)
    {
        std::vector<std::string> methods = route.route->allowedMethods;

        for (size_t i = 0; i < methods.size(); i++)
        {
            if (request.method == methods[i])
                return true;
        }
        
        return false;
    }

    std::string RouteResolver::getAllowedMethods(const ResolvedRoute& route)
    {
        std::ostringstream stream;

        stream << "\"";

        for (size_t i = 0; i < route.route->allowedMethods.size(); i++)
        {
            if (i != 0)
                stream << ",";

            stream << route.route->allowedMethods[i];
        }

        stream << "\"";

        return stream.str();        
    }

    void    RouteResolver::resolvePhisicalPath(ResolvedRoute& route)
    {
        if (!verifyAddressSegments(route.path))
        {
            route.phisicalPath.clear();
            route.httpStatus = 403;
            return;
        }

        route.phisicalPath = route.route->root + route.path;
        return;
    }

    bool    RouteResolver::verifyAddressSegments(const std::string& path)
    {
        std::istringstream stream (path);
        std::vector<std::string> pathElements;
        std::string element;

        while (std::getline(stream, element, '/'))
        {
            if (element.empty() || element == ".")
                continue;

            if (element == "..")
            {
                if (pathElements.empty())
                    return false;

                pathElements.pop_back();
            }
            else
                pathElements.push_back(element);
        }

        return true;
    }
}