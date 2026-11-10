# include "ModuleInterfaces.hpp" 

namespace webserv
{
    struct HttpParser::Impl
    {
        HttpRequest request;
        ParseResult parseResult;
        std::string data;
    };

    HttpParser::HttpParser(std::size_t maxBodyBytes) : _maxBodyBytes(maxBodyBytes) , 
        _impl(new Impl()) {}
    
    HttpParser::~HttpParser() {
        delete _impl;
    }

    ParseResult HttpParser::feed(const char* data, std::size_t size) {

    }
}