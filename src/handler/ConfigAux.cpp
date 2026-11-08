#include <sstream>
#include <stdexcept>
#include "ConfigAux.hpp"

namespace webserv
{
    void throwExpect(const Token& token, const std::string& msg)
    {
        std::ostringstream error;

        error << "line " << token.line << ": " << msg;
        throw std::runtime_error(error.str());
    }

    bool listenersConflit(const ListenAddress& a, const ListenAddress& b)
    {
        if (a.port == b.port && (a.host == b.host
                || a.host == "0.0.0.0"
                || b.host == "0.0.0.0"))
        {
            return true;
        }

        return false;
    }
}
