#include "ServerParser.hpp"
#include <sstream>
#include <stdexcept>
#include <cctype>

namespace webserv
{
    ServerConfig    ServerParser::parse(TokenStream& input)
    {
        ServerConfig    server;
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

            else
            {
                std::ostringstream error;
                error << "line " << input.peek().line
                    << ": invalid wordkey " << input.peek().value;
                throw std::runtime_error(error.str());
            }
        }

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
            throw std::runtime_error("listen: lack ':'");

        ip = token.value.substr(0, pos);
        port = token.value.substr(pos + 1);

        if (ip.size() == 0 || port.size() == 0)
        {
            std::ostringstream error;
            error << "line " << token.line
                << ": expected host:port";
            throw std::runtime_error(error.str());
        }

        std::istringstream stream(ip);
        while (std::getline(stream, part, '.'))
        {
            int num;
            std::stringstream ss(part);

            for (size_t i = 0; i < part.size(); ++i)
            {
                if (!std::isdigit(static_cast<unsigned char>(part[i])))
                    throw std::runtime_error("listen: invalid host");
            }

            ss >> num;
            if (ss.fail())
                throw std::runtime_error("listen: failure to read ip");

            if (num < 0 || num > 255)
                throw std::runtime_error("listen: host with an incorrect range");

            octets++;
        }

        if (ip[ip.size() - 1] == '.')
            throw std::runtime_error("listen: host cannot end with '.'");
        if (octets != 4)
            throw std::runtime_error("listen: expected four octets");

        
        std::stringstream ss(port);
        int numPort;

        for (size_t i = 0; i < port.size(); ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(port[i])))
                throw std::runtime_error("listen: invalid port");
        }

        ss >> numPort;
        if (ss.fail())
            throw std::runtime_error("listen: failure to read port");

        if (numPort < 1 || numPort > 65535)
            throw std::runtime_error("listen: port with an incorrect range");
           
        input.expect(";");

        ListenAddress listen;
        listen.host = ip;
        listen.port = static_cast<unsigned short>(numPort);
        server.listeners.push_back(listen);
    }
}