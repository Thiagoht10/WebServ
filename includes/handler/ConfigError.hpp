#ifndef CONFIG_ERROR_HPP
#define CONFIG_ERROR_HPP

#include "handler/config/ConfigLexer.hpp"
#include <string>

namespace webserv
{
    void throwExpect(const Token& token, const std::string& msg);
}

#endif
