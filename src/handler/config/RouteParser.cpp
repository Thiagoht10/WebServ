#include "RouteParser.hpp"
#include "handler/ConfigError.hpp"
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
                throwExpect(input.peek(), "invalid wordkey " + input.peek().value);
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
            throwExpect(token, "expected path");

        if (token.value[0] != '/')
            throwExpect(token, "bad path: expected /");
        
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
            throwExpect(token, "error_page: expected a path");

        if (!route.root.empty())
            throwExpect(token, "root alread defined");

        input.expect(";");
        route.root = token.value;
    }

    void    RouteParser::allowedMethods(TokenStream& input, RouteConfig& route)
    {
        Token token;
        std::vector<std::string>    tmp;

        input.expect("allowed_methods");

        if (!route.allowedMethods.empty())
            throwExpect(input.peek(), "parameter alread defined");

        if (input.peek().value == ";")
            throwExpect(input.peek(), "expected: GET, POST, or DELETE");
        
        while(input.peek().value != ";")
        {
            token = input.consume();
            
            if (token.value == "GET" || token.value == "POST"
                    || token.value == "DELETE")
            {
                for (size_t i = 0; i < tmp.size(); i++)
                {
                    if (token.value == tmp[i])
                        throwExpect(token, "repeated argument");
                }
                tmp.push_back(token.value);
            }
            else
                throwExpect(token, "expected: GET, POST, or DELETE");
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
            throwExpect(input.peek(), "parameter alread defined");

        token = input.consume();
        
        if (token.value == ";")
            throwExpect(token, "expected: 'on' or 'off' as argument");

        if (token.value != "on" && token.value != "off")
            throwExpect(token, "expected: 'on' or 'off' as argument");

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
            throwExpect(input.peek(), "parameter alread defined");

        token = input.consume();

        if (token.value == ";" || token.value == "{" || token.value == "}")
            throwExpect(token, "expected: an index file path");

        input.expect(";");

        route.indexFile = token.value;
    }

    void    RouteParser::uploadEnabled(TokenStream& input, RouteConfig& route)
    {
        Token   token;
        bool    result = false;

        input.expect("upload_enabled");

        if (_uploadEnableDefined)
            throwExpect(input.peek(), "parameter alread defined");

        token = input.consume();
        
        if (token.value == ";")
            throwExpect(token, "expected: a argument");

        if (token.value != "on" && token.value != "off")
            throwExpect(token, "expected: 'on' or 'off' as argument");

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
            throwExpect(input.peek(), "parameter alread defined");

        token = input.consume();

        if (token.value == ";" || token.value == "{" || token.value == "}")
            throwExpect(token, "expected: a path to directory");

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
            throwExpect(tokenKey, "expected: um extension file and path");

        if (tokenKey.value.size() < 2 || tokenKey.value[0] != '.')
            throwExpect(tokenKey, "cgi_interpreter: expected a file extension starting with '.' (e.g. '.py')");

        tokenValue = input.consume();

        if (tokenValue.value == ";" || tokenValue.value == "{"
                || tokenValue.value == "}")
            throwExpect(tokenValue, "expected: um extension file and path");

        input.expect(";");

        if (route.cgiInterpreters.find(tokenKey.value)
                != route.cgiInterpreters.end())
            throwExpect(tokenKey, "cgi_interpreter: extension already defined");
        
        route.cgiInterpreters[tokenKey.value] = tokenValue.value;
    }

    void    RouteParser::returnParser(TokenStream& input, RouteConfig& route)
    {
        Token tokenCode;
        Token tokenValue;

        input.expect("return");

        if (route.redirectStatus != 0)
            throwExpect(input.peek(), "return: already defined");

        tokenCode = input.consume();

        if (tokenCode.value == ";")
            throwExpect(tokenCode, "expected: a redirect status and a path");

        for (size_t i = 0; i < tokenCode.value.size(); i++)
        {
            if (!std::isdigit(static_cast<unsigned char>(tokenCode.value[i])))
                throwExpect(tokenCode, "expected: only digits on redirect status");
        }

        int code;
        std::stringstream stream(tokenCode.value);

        stream >> code;
        if (stream.fail())
            throwExpect(tokenCode, "failure to read digits");

        if (code != 301 && code != 302 && code != 303
                && code != 307 && code != 308)
            throwExpect(tokenCode, "return: expected 301, 302, 303, 307 or 308");

        tokenValue = input.consume();

        if (tokenValue.value == ";" || tokenValue.value == "{"
                || tokenValue.value == "}")
            throwExpect(tokenValue, "expected: a redirect status and a path");

        if (tokenValue.value[0] != '/')
            throwExpect(tokenValue, "bad path: expected /");

        input.expect(";");

        route.redirectStatus = code;
        route.redirectTarget = tokenValue.value;
    }

    void    RouteParser::verifyUpload(TokenStream& input, const RouteConfig& route)
    {
        if (route.uploadEnabled && route.uploadDirectory.empty())
            throwExpect(input.peek(), "it's need define 'upload_directory'");

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
