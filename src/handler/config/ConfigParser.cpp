#include "ConfigParser.hpp"
#include "ServerParser.hpp"
#include <stdexcept>
#include <sstream>

namespace webserv
{
    std::vector<ServerConfig>   ConfigParser::parse(TokenStream& input)
    {
        std::vector<ServerConfig>   servers;
        ServerParser                parser;

        while(!input.atEnd())
        {
            if (input.peek().value != "server")
            {
                std::ostringstream error;
                error << "line " << input.peek().line
                    << ": expected \"server\"";
                throw std::runtime_error(error.str());
            }

            ServerConfig server = parser.parse(input);
            servers.push_back(server);
        }

        return servers;        
    }
}