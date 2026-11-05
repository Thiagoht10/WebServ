#include "ServerParser.hpp"
#include "RouteParser.hpp"
#include "handler/ConfigError.hpp"
#include <sstream>
#include <stdexcept>
#include <cctype>


namespace webserv
{
    ServerParser::ServerParser()
        : _numMaxBodyDefined(false)
    {}

    ServerParser::~ServerParser(){}

    bool    ServerParser::onlyDigit(const std::string& value) const
    {
        if (value.empty())
            return false;

        for (size_t i = 0; i < value.size(); ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(value[i])))
                return false;
        }
        return true;
    }

    ServerConfig    ServerParser::parse(TokenStream& input)
    {
        ServerConfig    server;
        RouteParser     route;
        Token           token;

        token = input.consume();
        input.expect("{");

        while (input.peek().value != "}")
        {
            if(input.peek().value == "listen")
                parseListen(input, server);
            
            else if (input.peek().value == "client_max_body_size")
                maxBody(input, server);

            else if (input.peek().value == "error_page")
                errorPage(input, server);

            else if (input.peek().value == "location")
                server.routes.push_back(route.parse(input));

            else
                throwExpect(input.peek(), "invalid wordkey " + input.peek().value);
        }

        input.expect("}");

        return server;
    }

    void    ServerParser::parseListen(TokenStream& input, ServerConfig& server)
    {
        Token   token;
        std::string ip;
        std::string port;
        std::string part;
        size_t  octets = 0;

        input.expect("listen");
        token = input.consume();

        size_t pos = token.value.find(':');
        if (pos == std::string::npos)
            throwExpect(token, "expected ':'");

        ip = token.value.substr(0, pos);
        port = token.value.substr(pos + 1);

        if (ip.size() == 0 || port.size() == 0)
            throwExpect(token, "expected host:port");

        std::istringstream stream(ip);
        while (std::getline(stream, part, '.'))
        {
            int octetValue;
            std::stringstream ss(part);

            if (!onlyDigit(part))
                throwExpect(token, "listen: invalid host");

            ss >> octetValue;
            if (ss.fail())
                throwExpect(token, "listen: failure to read ip");

            if (octetValue < 0 || octetValue > 255)
                throwExpect(token, "listen: host with an incorrect range");

            octets++;
        }

        if (ip[ip.size() - 1] == '.')
            throwExpect(token, "listen: host cannot end with '.'");
        if (octets != 4)
            throwExpect(token, "listen: expected four octets");

        
        std::stringstream ss(port);
        int portNumber;

        if (!onlyDigit(port))
            throwExpect(token, "listen: invalid port");

        ss >> portNumber;
        if (ss.fail())
            throwExpect(token, "listen: failure to read port");

        if (portNumber < 1 || portNumber > 65535)
            throwExpect(token, "listen: port with an incorrect range");
           
        input.expect(";");

        ListenAddress listen;
        listen.host = ip;
        listen.port = port;
        server.listeners.push_back(listen);
    }

    void    ServerParser::maxBody(TokenStream& input, ServerConfig& server)
    {
        Token token;

        input.expect("client_max_body_size");
        if (_numMaxBodyDefined)
            throwExpect(input.peek(), "client_max_body_size alread defined");

        token = input.consume();

        if (token.value == ";")
            throwExpect(token, "max_body: is empty");

        if (!onlyDigit(token.value))
            throwExpect(token, "expected only numbers");

        size_t  convertMaxBody;
        std::stringstream stream(token.value);

        stream >> convertMaxBody;
        if (stream.fail())
            throwExpect(token, "failure to read value");

        input.expect(";");

        server.maxBodyBytes = convertMaxBody;
        _numMaxBodyDefined = true;
    }

    void    ServerParser::errorPage(TokenStream& input, ServerConfig& server)
    {
        Token   tokenCode;
        Token   tokenPath;

        input.expect("error_page");
        tokenCode = input.consume();

        if (!onlyDigit(tokenCode.value))
            throwExpect(tokenCode, "expected only numbers");

        int convertErrorPage;
        std::stringstream stream(tokenCode.value);

        stream >> convertErrorPage;
        if (stream.fail())
            throwExpect(tokenCode, "failure to read value");

        if (server.errorPages.find(convertErrorPage) != server.errorPages.end())
            throwExpect(tokenCode, "error_page: code alread defined");

        if (convertErrorPage < 400 || convertErrorPage > 599)
            throwExpect(tokenCode, "error_page: expected code between 400 and 599");

        tokenPath = input.consume();

        if (tokenPath.value.empty()
            || tokenPath.value == ";"
            || tokenPath.value == "{"
            || tokenPath.value == "}")
            throwExpect(tokenPath, "error_page: expected a path");

        input.expect(";");

        server.errorPages[convertErrorPage] = tokenPath.value;
    }
}
