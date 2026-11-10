# include "ModuleInterfaces.hpp"
# include <string>

namespace webserv
{
    struct HttpParser::Impl
    {
        HttpRequest request;
        ParseResult parseResult;

        std::string data;

        std::string requestLine;
        std::string method;
        std::string target;
        std::string version;

        std::size_t headerEnd;
        std::size_t bodyStart;
        std::size_t requestStart;
        std::size_t requestEnd;
        std::size_t lineStart;
        std::size_t lineEnd;
        std::size_t lineLength;

        bool    foundHeaderEnd;
    };

    HttpParser::HttpParser(std::size_t maxBodyBytes) : _maxBodyBytes(maxBodyBytes), 
        _impl(new Impl()) {
        _impl->foundHeaderEnd = false;
        _impl->headerEnd = 0;
        _impl->bodyStart = 0;
        _impl->requestStart = 0;
        _impl->requestEnd = 0;
        _impl->lineStart = 0;
        _impl->lineEnd = 0;
        _impl->lineLength = 0;
    }
    
    HttpParser::~HttpParser() {
        delete _impl;
    }

    ParseResult HttpParser::feed(const char* data, std::size_t size) {
        _impl->data.append(data, size);
        if (!_impl->foundHeaderEnd)
        {
            _impl->headerEnd = _impl->data.find("\r\n\r\n");
            if (_impl->headerEnd == std::string::npos)
            {
                _impl->parseResult.state = PARSE_NEED_MORE;
                return _impl->parseResult;
            }
            _impl->bodyStart = _impl->headerEnd + 4;
            _impl->foundHeaderEnd = true;
        }
        if (!parseRequestLine())
            return _impl->parseResult;
    }

    bool    HttpParser::parseRequestLine() {
        _impl->requestEnd = _impl->data.find("\r\n", _impl->requestStart);
        if (_impl->requestEnd == std::string::npos)
        {
            _impl->parseResult.state = PARSE_NEED_MORE;
            return false;
        }
        _impl->requestLine = _impl->data.substr(_impl->requestStart , _impl->requestEnd - _impl->requestStart);
        if (_impl->requestLine.find('\t') != std::string::npos)
        {
            _impl->parseResult.state = PARSE_ERROR;
            return false;
        }
        _impl->lineEnd = _impl->requestLine.find(" ");
        if (_impl->lineEnd == std::string::npos || _impl->lineEnd == 0)
        {
            _impl->parseResult.state = PARSE_ERROR;
            return false;
        }
        _impl->lineLength = _impl->lineEnd - _impl->lineStart;
        _impl->method = _impl->requestLine.substr(_impl->lineStart, _impl->lineLength);

        _impl->lineStart = _impl->lineEnd + 1;
        _impl->lineEnd = _impl->requestLine.find(" ", _impl->lineStart);
        if (_impl->lineEnd == std::string::npos || _impl->lineEnd == _impl->lineStart)
        {
            _impl->parseResult.state = PARSE_ERROR;
            return false;
        }
        _impl->lineLength = _impl->lineEnd - _impl->lineStart;
        _impl->target = _impl->requestLine.substr(_impl->lineStart, _impl->lineLength);

        _impl->lineStart = _impl->lineEnd + 1;
        if (_impl->lineStart >= _impl->requestLine.size())
        {
            _impl->parseResult.state = PARSE_ERROR;
            return false;
        }
        _impl->version = _impl->requestLine.substr(_impl->lineStart);
        if (_impl->version.find(' ') != std::string::npos)
        {
            _impl->parseResult.state = PARSE_ERROR;
            return false;
        }
        return true;
    }
}