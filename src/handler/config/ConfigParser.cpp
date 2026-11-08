#include "ConfigParser.hpp"
#include "ServerParser.hpp"
#include "ConfigAux.hpp"
#include <stdexcept>
#include <sstream>

namespace webserv
{
    std::vector<ServerConfig>   ConfigParser::parse(TokenStream& input)
    {
        std::vector<ServerConfig>   servers;

        while(!input.atEnd())
        {
            if (input.peek().value != "server")
            {
                std::ostringstream error;
                error << "line " << input.peek().line
                    << ": expected \"server\", found \"" << input.peek().value << "\"";
                throw std::runtime_error(error.str());
            }

            ServerParser                parser;
            ServerConfig server = parser.parse(input);
            servers.push_back(server);
        }

        if (servers.empty())
            throw std::runtime_error("server: at least one defined server is expected.");

        verifyServerListeners(servers);
        
        return servers;
    }

    void    ConfigParser::verifyServerListeners(const std::vector<ServerConfig>& servers)
    {
        for (size_t i = 0; i < servers.size(); i++)
        {
            for (size_t j = i + 1; j < servers.size(); j++)
            {
                for (size_t a = 0; a < servers[i].listeners.size(); a++)
                {
                    for (size_t b = 0; b < servers[j].listeners.size(); b++)
                    {
                        if (listenersConflit(servers[i].listeners[a],
                                servers[j].listeners[b]))
                        {
                            throw std::runtime_error(
                                "listen: conflicting listeners between servers");
                        }
                    }
                }
            }
        }
    }
}