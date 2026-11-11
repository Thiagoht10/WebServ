#include "ModuleInterfaces.hpp"
#include "ServerConfig.hpp"
#include "RouteResolver.hpp"
#include <sstream>
#include <fstream>

namespace webserv
{
    struct  RequestHandler::Impl
    {
        ResolvedRoute   route;
    };

    RequestHandler::RequestHandler()
        : _impl(new Impl())
    {}

    RequestHandler::~RequestHandler()
    {
        delete _impl;
    }

    HandlerResult   RequestHandler::handle(const HttpRequest& request,
            const ServerConfig& config)
    {
        RouteResolver   resolver;
        HandlerResult   result;

        _impl->route = resolver.resolve(request, config);

        if (_impl->route.httpStatus != 0)
        {
            result.state = HANDLER_ERROR;
            result.response = makeError(_impl->route.httpStatus, config);
            return result;
        }

        if (_impl->route.route->redirectStatus != 0)
            return resolver.redirect(_impl->route, request);

        if (!resolver.verifyAllowedMethods(_impl->route, request))
        {
            result.state = HANDLER_ERROR;
            result.response = makeError(405, config);

            HttpHeader header;
            header.name = "Allow";
            header.value = resolver.getAllowedMethods(_impl->route);

            result.response.headers.push_back(header);
            return result;
        }

        resolver.resolvePhisicalPath(_impl->route);
        if (_impl->route.httpStatus != 0)
        {
            result.state = HANDLER_ERROR;
            result.response = makeError(403, config);
            return result;
        }
    }

    ResponseData    RequestHandler::makeError(int statusCode,
            const ServerConfig& config) const
    {
        ResponseData        response;
        std::ostringstream  html;
        std::string         path;
        std::string         content;

        response.closeConnection = true;
        response.statusCode = statusCode;

        html << "<!DOCTYPE html>" 
            << "<html><head><title>" << statusCode << "</title></head>"
            << "<body><h1>" << statusCode << "</h1></body></html>";

        std::map<int, std::string>::const_iterator it;

        content = html.str();

        it = config.errorPages.find(statusCode);
        if (it != config.errorPages.end())
        {
            path = it->second;

            std::ifstream file(path.c_str(), file.binary);
            if (!file.is_open())
            {
                std::ostringstream buffer;
                buffer << file.rdbuf();

                if (!buffer.bad())
                    content = buffer.str();
            }
        }

        HttpHeader header;

        header.name = "Content-Type";
        header.value = "text/html";

        response.body = content;
        response.headers.push_back(header);

        return response;
    }
}