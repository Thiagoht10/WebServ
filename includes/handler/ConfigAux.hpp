#ifndef CONFIG_AUX_HPP
#define CONFIG_AUX_HPP

#include "ConfigLexer.hpp"
#include "ServerConfig.hpp"
#include <string>

namespace webserv
{
    void throwExpect(const Token& token, const std::string& msg);
    bool listenersConflit(const ListenAddress& a, const ListenAddress& b);
}

#endif
