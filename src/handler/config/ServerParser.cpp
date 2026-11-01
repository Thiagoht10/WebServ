#include "ServerParser.hpp"
#include <sstream>
#include <stdexcept>
#include <cctype>


namespace webserv
{
    ServerParser::ServerParser()
        : numMaxBodyDefined(0)
    {}

    ServerParser::~ServerParser(){}

    void    ServerParser::throwExpect(const Token& token, const std::string& msg)
    {
        std::ostringstream error;

        error << "line " << token.line << ": " << msg;
        throw std::runtime_error(error.str());
    }

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
                throwExpect(input.peek(), "invalid wordkey " + input.peek().value);
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

            for (size_t i = 0; i < part.size(); ++i)
            {
                if (!std::isdigit(static_cast<unsigned char>(part[i])))
                    throwExpect(token, "listen: invalid host");
            }

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

        for (size_t i = 0; i < port.size(); ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(port[i])))
                throwExpect(token, "listen: invalid port");
        }

        ss >> portNumber;
        if (ss.fail())
            throwExpect(token, "listen: failure to read port");

        if (portNumber < 1 || portNumber > 65535)
            throwExpect(token, "listen: port with an incorrect range");
           
        input.expect(";");

        ListenAddress listen;
        listen.host = ip;
        listen.port = static_cast<unsigned short>(portNumber);
        server.listeners.push_back(listen);
    }

    void    ServerParser::maxBody(TokenStream& input, ServerConfig& server)
    {
        Token token;

        input.expect("client_max_body_size");
        if (numMaxBodyDefined != 0)
            throwExpect(input.peek(), "client_max_body_size alread defined");

        token = input.consume();

        if (token.value == ";")
            throwExpect(token, "max_body: is empty");

        for (size_t i = 0; i < token.value.size(); i++)
        {
            if (!std::isdigit(static_cast<unsigned char>(token.value[i])))
                throwExpect(token, "expected only numbers");
        }

        size_t  convertMaxBody;
        std::stringstream stream(token.value);

        stream >> convertMaxBody;
        if (stream.fail())
            throwExpect(token, "failure to read value");

        input.expect(";");

        server.maxBodyBytes = convertMaxBody;
        numMaxBodyDefined++;
    }
}