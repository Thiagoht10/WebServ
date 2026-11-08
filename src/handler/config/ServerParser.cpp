#include "ServerParser.hpp"
#include "RouteParser.hpp"
#include "ConfigAux.hpp"
#include <sstream>
#include <stdexcept>
#include <cctype>


namespace webserv
{
    ServerParser::ServerParser()
        : _maxBodyDefined(false)
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

        _maxBodyDefined = false;

        input.expect("server");
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
                throwExpect(input.peek(), "unknown directive '" + input.peek().value + "' in server block");
        }

        verifyListeners(input, server);
        verifyRoutes(input, server);

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
            throwExpect(token, "listen: expected an IPv4 address and port separated by ':'");

        ip = token.value.substr(0, pos);
        port = token.value.substr(pos + 1);

        if (ip.size() == 0 || port.size() == 0)
            throwExpect(token, "listen: expected a nonempty IPv4 address and port (e.g. 127.0.0.1:8080)");

        if (ip[ip.size() - 1] == '.')
            throwExpect(token, "listen: IPv4 address must not end with '.'");

        std::istringstream stream(ip);
        ip.clear();
        while (std::getline(stream, part, '.'))
        {
            int octetValue;
            std::stringstream ss(part);

            if (!onlyDigit(part))
                throwExpect(token, "listen: each IPv4 octet must contain only decimal digits");

            ss >> octetValue;
            if (ss.fail())
                throwExpect(token, "listen: each IPv4 octet must be between 0 and 255");

            if (octetValue < 0 || octetValue > 255)
                throwExpect(token, "listen: each IPv4 octet must be between 0 and 255");

            std::stringstream ss2;
            ss2 << octetValue;
            if (octets > 0)
                ip.append(".");
            ip.append(ss2.str());
            octets++;
        }

        if (octets != 4)
            throwExpect(token, "listen: IPv4 address must contain exactly four octets");

        
        std::stringstream ss(port);
        int portNumber;

        if (!onlyDigit(port))
            throwExpect(token, "listen: port must contain only decimal digits");

        ss >> portNumber;
        if (ss.fail())
            throwExpect(token, "listen: port must be between 1 and 65535");

        if (portNumber < 1 || portNumber > 65535)
            throwExpect(token, "listen: port must be between 1 and 65535");
           
        input.expect(";");

        std::stringstream ss2;
        ss2 << portNumber;
        port = ss2.str();

        ListenAddress listen;
        listen.host = ip;
        listen.port = port;
        server.listeners.push_back(listen);
    }

    void    ServerParser::maxBody(TokenStream& input, ServerConfig& server)
    {
        Token token;

        input.expect("client_max_body_size");
        if (_maxBodyDefined)
            throwExpect(input.peek(), "client_max_body_size: directive already defined in this server block");

        token = input.consume();

        if (token.value == ";")
            throwExpect(token, "client_max_body_size: expected a nonnegative integer in bytes");

        if (!onlyDigit(token.value))
            throwExpect(token, "client_max_body_size: expected a nonnegative integer in bytes");

        size_t  convertMaxBody;
        std::stringstream stream(token.value);

        stream >> convertMaxBody;
        if (stream.fail())
            throwExpect(token, "client_max_body_size: value exceeds the supported size range");

        input.expect(";");

        server.maxBodyBytes = convertMaxBody;
        _maxBodyDefined = true;
    }

    void    ServerParser::errorPage(TokenStream& input, ServerConfig& server)
    {
        Token   tokenCode;
        Token   tokenPath;

        input.expect("error_page");
        tokenCode = input.consume();

        if (!onlyDigit(tokenCode.value))
            throwExpect(tokenCode, "error_page: status code must contain only decimal digits");

        int convertErrorPage;
        std::stringstream stream(tokenCode.value);

        stream >> convertErrorPage;
        if (stream.fail())
            throwExpect(tokenCode, "error_page: status code exceeds the supported integer range");

        if (server.errorPages.find(convertErrorPage) != server.errorPages.end())
            throwExpect(tokenCode, "error_page: status code already defined in this server block");

        if (convertErrorPage < 400 || convertErrorPage > 599)
            throwExpect(tokenCode, "error_page: expected an HTTP status code between 400 and 599");

        tokenPath = input.consume();

        if (tokenPath.value.empty()
            || tokenPath.value == ";"
            || tokenPath.value == "{"
            || tokenPath.value == "}")
            throwExpect(tokenPath, "error_page: expected an error page file path");

        input.expect(";");

        server.errorPages[convertErrorPage] = tokenPath.value;
    }

    void    ServerParser::verifyListeners(const TokenStream& input, const ServerConfig& server)
    {
        if (server.listeners.empty())
            throwExpect(input.peek(), "listen: at least one listen is expected");

        for (size_t i = 0; i < server.listeners.size(); i++)
        {
            for (size_t j = i + 1; j < server.listeners.size(); j++)
            {
                if (listenersConflit(server.listeners[i], server.listeners[j]))
                    throwExpect(input.peek(), "listen: there is a listen conflict");
            }
        }
    }

    void    ServerParser::verifyRoutes(const TokenStream& input, const ServerConfig& server)
    {
        if (server.routes.empty())
            throwExpect(input.peek(), "location: at least one route must be defined");
        
        size_t  size = server.routes.size();

        for (size_t i = 0; i < size; i++)
        {
            for (size_t j = i + 1; j < size; j++)
            {
                if (server.routes[i].path == server.routes[j].path)
                {
                    throwExpect(input.peek(), 
                        "location: there is a conflict in the routes");
                }
            }
        }
    }
}
