#include "handler/ConfigError.hpp"
#include <sstream>
#include <stdexcept>

namespace webserv
{
    void throwExpect(const Token& token, const std::string& msg)
    {
        std::ostringstream error;

        error << "line " << token.line << ": " << msg;
        throw std::runtime_error(error.str());
    }
}
