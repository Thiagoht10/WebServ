#include "RouteParser.hpp"
#include "ConfigAux.hpp"
#include <cctype>
#include <sstream>

namespace webserv
{
    RouteParser::RouteParser()
        : _directListeningDefined(false), _uploadEnableDefined(false)
    {}

    RouteParser::~RouteParser()
    {}

    RouteConfig RouteParser::parse(TokenStream& input)
    {
        RouteConfig route;
        Token       token;

        _directListeningDefined = false;
        _uploadEnableDefined = false;

        input.expect("location");
        path(input, route);
        input.expect("{");

        while (input.peek().value != "}")
        {
            if (input.peek().value == "root")
                root(input, route);

            else if (input.peek().value == "allowed_methods")
                allowedMethods(input, route);

            else if (input.peek().value == "directory_listing")
                directoryListing(input, route);

            else if (input.peek().value == "index")
                index(input, route);

            else if (input.peek().value == "upload_enabled")
                uploadEnabled(input, route);

            else if (input.peek().value == "upload_directory")
                uploadDirectory(input, route);

            else if (input.peek().value == "cgi_interpreter")
                cgiInterpreter(input, route);

            else if (input.peek().value == "return")
                returnParser(input, route);

            else
                throwExpect(input.peek(), "unknown directive '" + input.peek().value + "' in location block");
        }

        verifyAllowedMethods(route);
        verifyUpload(input, route);
        verifyRoot(input, route);
        verifyCombination(input, route);

        input.expect("}");
        return route;
    }

    void    RouteParser::path(TokenStream& input, RouteConfig& route)
    {
        Token   token;

        token = input.consume();

        if (token.value == "{" || token.value == "}" || token.value == ";")
            throwExpect(token, "location: expected a path starting with '/'");

        if (token.value[0] != '/')
            throwExpect(token, "location: path must start with '/'");
        
        route.path = token.value;
    }

    void    RouteParser::root(TokenStream& input, RouteConfig& route)
    {
        Token token;

        input.expect("root");
        token = input.consume();

        if (token.value.empty()
            || token.value == ";"
            || token.value == "{"
            || token.value == "}")
            throwExpect(token, "root: expected a directory path");

        if (!route.root.empty())
            throwExpect(token, "root: directive already defined in this location block");

        input.expect(";");
        route.root = token.value;
    }

    void    RouteParser::allowedMethods(TokenStream& input, RouteConfig& route)
    {
        Token token;
        std::vector<std::string>    tmp;

        input.expect("allowed_methods");

        if (!route.allowedMethods.empty())
            throwExpect(input.peek(), "allowed_methods: directive already defined in this location block");

        if (input.peek().value == ";")
            throwExpect(input.peek(), "allowed_methods: expected one or more methods (GET, POST, DELETE)");
        
        while(input.peek().value != ";")
        {
            token = input.consume();
            
            if (token.value == "GET" || token.value == "POST"
                    || token.value == "DELETE")
            {
                for (size_t i = 0; i < tmp.size(); i++)
                {
                    if (token.value == tmp[i])
                        throwExpect(token, "allowed_methods: duplicate method");
                }
                tmp.push_back(token.value);
            }
            else
                throwExpect(token, "allowed_methods: expected one or more methods (GET, POST, DELETE)");
        }

        input.expect(";");

        route.allowedMethods = tmp;
    }

    void    RouteParser::directoryListing(TokenStream& input, RouteConfig& route)
    {
        Token   token;
        bool    result = false;

        input.expect("directory_listing");
        
        if (_directListeningDefined)
            throwExpect(input.peek(), "directory_listing: directive already defined in this location block");

        token = input.consume();
        
        if (token.value == ";")
            throwExpect(token, "directory_listing: expected 'on' or 'off'");

        if (token.value != "on" && token.value != "off")
            throwExpect(token, "directory_listing: expected 'on' or 'off'");

        input.expect(";");
        
        if (token.value == "on")
            result = true;
        
        route.directoryListing = result;
        _directListeningDefined = true;
    }

    void    RouteParser::index(TokenStream& input, RouteConfig& route)
    {
        Token token;

        input.expect("index");

        if (!route.indexFile.empty())
            throwExpect(input.peek(), "index: directive already defined in this location block");

        token = input.consume();

        if (token.value == ";" || token.value == "{" || token.value == "}")
            throwExpect(token, "index: expected an index file path");

        input.expect(";");

        route.indexFile = token.value;
    }

    void    RouteParser::uploadEnabled(TokenStream& input, RouteConfig& route)
    {
        Token   token;
        bool    result = false;

        input.expect("upload_enabled");

        if (_uploadEnableDefined)
            throwExpect(input.peek(), "upload_enabled: directive already defined in this location block");

        token = input.consume();
        
        if (token.value == ";")
            throwExpect(token, "upload_enabled: expected 'on' or 'off'");

        if (token.value != "on" && token.value != "off")
            throwExpect(token, "upload_enabled: expected 'on' or 'off'");

        input.expect(";");

        if (token.value == "on")
            result = true;

        route.uploadEnabled = result;
        _uploadEnableDefined = true;
    }

    void    RouteParser::uploadDirectory(TokenStream& input, RouteConfig& route)
    {
        Token token;

        input.expect("upload_directory");

        if (!route.uploadDirectory.empty())
            throwExpect(input.peek(), "upload_directory: directive already defined in this location block");

        token = input.consume();

        if (token.value == ";" || token.value == "{" || token.value == "}")
            throwExpect(token, "upload_directory: expected a directory path");

        input.expect(";");

        route.uploadDirectory = token.value;
    }

    void    RouteParser::cgiInterpreter(TokenStream& input, RouteConfig& route)
    {
        Token   tokenKey;
        Token   tokenValue;

        input.expect("cgi_interpreter");

        tokenKey = input.consume();

        if (tokenKey.value == ";")
            throwExpect(tokenKey, "cgi_interpreter: expected a file extension and an interpreter path");

        if (tokenKey.value.size() < 2 || tokenKey.value[0] != '.')
            throwExpect(tokenKey, "cgi_interpreter: expected a file extension starting with '.' (e.g. '.py')");

        tokenValue = input.consume();

        if (tokenValue.value == ";" || tokenValue.value == "{"
                || tokenValue.value == "}")
            throwExpect(tokenValue, "cgi_interpreter: expected a file extension and an interpreter path");

        input.expect(";");

        if (route.cgiInterpreters.find(tokenKey.value)
                != route.cgiInterpreters.end())
            throwExpect(tokenKey, "cgi_interpreter: file extension already defined in this location block");
        
        route.cgiInterpreters[tokenKey.value] = tokenValue.value;
    }

    void    RouteParser::returnParser(TokenStream& input, RouteConfig& route)
    {
        Token tokenCode;
        Token tokenValue;

        input.expect("return");

        if (route.redirectStatus != 0)
            throwExpect(input.peek(), "return: directive already defined in this location block");

        tokenCode = input.consume();

        if (tokenCode.value == ";")
            throwExpect(tokenCode, "return: expected a redirect status code and a target path");

        for (size_t i = 0; i < tokenCode.value.size(); i++)
        {
            if (!std::isdigit(static_cast<unsigned char>(tokenCode.value[i])))
                throwExpect(tokenCode, "return: status code must contain only decimal digits");
        }

        int code;
        std::stringstream stream(tokenCode.value);

        stream >> code;
        if (stream.fail())
            throwExpect(tokenCode, "return: status code exceeds the supported integer range");

        if (code != 301 && code != 302 && code != 303
                && code != 307 && code != 308)
            throwExpect(tokenCode, "return: expected status code 301, 302, 303, 307 or 308");

        tokenValue = input.consume();

        if (tokenValue.value == ";" || tokenValue.value == "{"
                || tokenValue.value == "}")
            throwExpect(tokenValue, "return: expected a redirect status code and a target path");

        if (tokenValue.value[0] != '/')
            throwExpect(tokenValue, "return: target path must start with '/'");

        input.expect(";");

        route.redirectStatus = code;
        route.redirectTarget = tokenValue.value;
    }

    void    RouteParser::verifyUpload(TokenStream& input, const RouteConfig& route)
    {
        if (route.uploadEnabled && route.uploadDirectory.empty())
            throwExpect(input.peek(), "upload_enabled: 'on' requires an upload_directory directive");

        if (route.uploadEnabled)
        {
            for (size_t i = 0; i < route.allowedMethods.size(); i++)
            {
                if (route.allowedMethods[i] == "POST")
                    return;
            }
            throwExpect(input.peek(),
                "upload_enabled: requires POST in allowed_methods");
        }
    }

    void    RouteParser::verifyRoot(TokenStream& input, const RouteConfig& route)
    {
        if (route.root.empty())
        {
            if (route.redirectStatus == 0 || !route.indexFile.empty()
                    || route.uploadEnabled
                    || route.directoryListing 
                    || !route.cgiInterpreters.empty())
            {
                throwExpect(input.peek(), 
                    "root: required for file operations or CGI");
            }
        }
    }

    void    RouteParser::verifyAllowedMethods(RouteConfig& route)
    {
        if (route.allowedMethods.empty())
            route.allowedMethods.push_back("GET");
    }

    void    RouteParser::verifyCombination(TokenStream& input, const RouteConfig& route)
    {
        if (route.redirectStatus != 0)
        {
            if (!route.indexFile.empty() || route.uploadEnabled
                    || route.directoryListing
                    || !route.cgiInterpreters.empty())
            {
                throwExpect(input.peek(),
                    "return: cannot be combined with index, upload_enabled on, "
                    "directory_listing on or cgi_interpreter");
            }
        }
    }
}
