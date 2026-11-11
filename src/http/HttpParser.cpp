# include "../../includes/ModuleInterfaces.hpp"
# include <string>

namespace webserv
{
    struct HttpParser::Impl
    {
        HttpRequest httpRequest;
        ParseResult parseResult;

        std::string data;

        std::string requestLine;
        std::string headerLine;

        std::size_t headerStart;
        std::size_t headerEnd;
        std::size_t headerSeparator;
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
        _impl->headerStart = 0;
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
        if (!parseRequestLine() || !parseHeaders())
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
        _impl->httpRequest.method = _impl->requestLine.substr(_impl->lineStart, _impl->lineLength);

        _impl->lineStart = _impl->lineEnd + 1;
        _impl->lineEnd = _impl->requestLine.find(" ", _impl->lineStart);
        if (_impl->lineEnd == std::string::npos || _impl->lineEnd == _impl->lineStart)
        {
            _impl->parseResult.state = PARSE_ERROR;
            return false;
        }
        _impl->lineLength = _impl->lineEnd - _impl->lineStart;
        _impl->httpRequest.target = _impl->requestLine.substr(_impl->lineStart, _impl->lineLength);

        _impl->lineStart = _impl->lineEnd + 1;
        if (_impl->lineStart >= _impl->requestLine.size())
        {
            _impl->parseResult.state = PARSE_ERROR;
            return false;
        }
        _impl->httpRequest.version = _impl->requestLine.substr(_impl->lineStart);
        if (_impl->httpRequest.version.find(' ') != std::string::npos)
        {
            _impl->parseResult.state = PARSE_ERROR;
            return false;
        }
        return true;
    }

    bool    HttpParser::parseHeaders() {
        HttpHeader header;
        _impl->headerStart = _impl->requestEnd + 2;
        _impl->lineStart = _impl->headerStart;
        while (_impl->headerStart < _impl->headerEnd)
        {
            _impl->lineEnd = _impl->data.find("\r\n", _impl->lineStart);
            if (_impl->lineEnd == std::string::npos || _impl->lineEnd > _impl->headerEnd)
            {
                _impl->parseResult.state = PARSE_ERROR;
                return false;
            }
            _impl->lineLength = _impl->lineEnd - _impl->lineStart;
            _impl->headerLine = _impl->data.substr(_impl->lineStart, _impl->lineLength);

            _impl->headerSeparator = _impl->headerLine.find(':');
            if (_impl->headerSeparator == std::string::npos || _impl->headerSeparator == 0)
            {
                _impl->parseResult.state = PARSE_ERROR;
                return false;
            }
            header.name = _impl->headerLine.substr(0, _impl->headerSeparator);

            header.value = _impl->headerLine.substr(_impl->headerSeparator + 1);
            _impl->httpRequest.headers.push_back(header);

            _impl->lineStart = _impl->lineEnd + 2;
            _impl->headerStart = _impl->lineEnd + 2;
        }
        if (!validateHeaders())
        {
            _impl->parseResult.state = PARSE_ERROR;
            return false;
        }
        return true;
    }

    bool    HttpParser::validateHeaders() {
        HttpHeaders::iterator it;
        std::size_t first;
        std::size_t last;

        for (it = _impl->httpRequest.headers.begin(); it != _impl->httpRequest.headers.end(); ++it)
        {
            if (it->name.find(' ') != std::string::npos || it->name.find('\t') != std::string::npos)
                return false;
            first = it->value.find_first_not_of(" \t");
            if (first == std::string::npos)
                it->value.clear();
            else
            {
                last = it->value.find_last_not_of(" \t");
                it->value = it->value.substr(first, last - first + 1);
            }
        }
        return true;
    }

    const HttpRequest&  HttpParser::request() const {
        return _impl->httpRequest;
    }
}
