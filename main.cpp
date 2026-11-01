#include "ServerConfig.hpp"
#include <exception>
#include <iostream>

int main(int argc, char *argv[])
{
    if (argc > 2)
    {
        std::cerr << "Usage: " << argv[0] << " [config_file]\n";
        return 1;
    }

    try
    {
        webserv::ConfigLoader loader;
        loader.load(argc == 2 ? argv[1] : "config/default.conf");
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
