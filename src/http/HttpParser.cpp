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
        bool    httpVersion;
    };

    HttpParser::HttpParser(std::size_t maxBodyBytes) : _maxBodyBytes(maxBodyBytes), 
        _impl(new Impl()) {
        _impl->foundHeaderEnd = false;
        _impl->httpVersion = false;
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

        _impl->parseResult.state = PARSE_COMPLETE;
        return _impl->parseResult;
    }

    bool    HttpParser::parseRequestLine() {
        // extrai a request line
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
        // comeca por encontrar o primeiro espaco para poder separar em seguida q request line em method, target e version
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

        if (_impl->httpRequest.method != "GET" && _impl->httpRequest.method != "POST" 
            && _impl->httpRequest.method != "DELETE")
        {
            _impl->parseResult.state = PARSE_ERROR;
            return false;
        }

        if (_impl->httpRequest.version == "HTTP/1.1")
            _impl->httpVersion = true;
        else if (_impl->httpRequest.version != "HTTP/1.0")
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
            //extrai um header
            _impl->lineEnd = _impl->data.find("\r\n", _impl->lineStart);
            if (_impl->lineEnd == std::string::npos || _impl->lineEnd > _impl->headerEnd)
            {
                _impl->parseResult.state = PARSE_ERROR;
                return false;
            }
            _impl->lineLength = _impl->lineEnd - _impl->lineStart;
            _impl->headerLine = _impl->data.substr(_impl->lineStart, _impl->lineLength);

            // divide o header em name e value
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
        if (!validateHeaders(_impl->httpRequest.headers))
        {
            _impl->parseResult.state = PARSE_ERROR;
            return false;
        }
        return true;
    }

    bool    HttpParser::validateHeaders(HttpHeaders&    headers) {
        HttpHeaders::iterator it;
        std::size_t hostCount = 0;
        std::size_t first;
        std::size_t last;

        for (it = headers.begin(); it != headers.end(); ++it)
        {
            if (it->name.empty())
                return false;
            // Da um trim no value de qualquer header se for preciso removendo trailing spaces e leading spaces
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

            // verifica se o value do header "HOST" e empty
            if (isHeaderName(it->name, "HOST") && it->value.empty() 
                && _impl->httpVersion)
            {
                _impl->parseResult.state = PARSE_ERROR;
                return false;
            }
            if (!isValidHeaderName(it->name))
                return false;
        }

        // verifica se existe um "HOST" na versao 1.1 e se existe mais de 1 header de "HOST"
        hostCount = countHeaderNameOccurrences(headers, "HOST");
        if (_impl->httpVersion && hostCount != 1)
        {
            _impl->parseResult.state = PARSE_ERROR;
            return false;
        }
        else if (!_impl->httpVersion && hostCount > 1)
        {
            _impl->parseResult.state = PARSE_ERROR;
            return false;
        }

        if (!isValidTransferEncodingHeader(headers))
            return false;
        return true;
    }

    std::size_t HttpParser::countHeaderNameOccurrences(const HttpHeaders& headers, const std::string& expected) {
        HttpHeaders::const_iterator it;
        std::size_t count = 0;

        for (it = headers.begin(); it != headers.end(); ++it)
        {
            if (isHeaderName(it->name, expected))
                count++;
        }
        return count;
    }

    bool    HttpParser::isHeaderName(std::string name, const std::string& expected) const {
        for (std::size_t i = 0; i < name.size(); i++)
            name[i] = std::toupper(static_cast<unsigned char>(name[i]));
        return (name == expected);
    }

    bool    HttpParser::isValidHeaderName(const std::string& name) const {
        if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!#$%&'*+-.^_`|~") 
            != std::string::npos)
        {
            _impl->parseResult.state = PARSE_ERROR;
            return false;
        }
        return true;
    }

    bool    HttpParser::isValidTransferEncodingHeader(const HttpHeaders&  headers) {
        // verificar se existe um header de "Transfer-Encoding" de acordo com a versao HTTP
        if (countHeaderNameOccurrences(headers, "TRANSFER-ENCODING") != 0 
            && !_impl->httpVersion)
        {
            _impl->parseResult.state = PARSE_ERROR;
            return false;
        }
        // verifica se existe "Transfer-Encoding" e "Content-Length" na mesma request
        if (countHeaderNameOccurrences(headers, "TRANSFER-ENCODING") != 0 
            && countHeaderNameOccurrences(headers, "CONTENT-LENGTH") != 0)
        {
            _impl->parseResult.state = PARSE_ERROR;
            return false;
        }
        return true;
    }

    bool    HttpParser::isValidContentLengthHeader(const HttpHeaders&    headers) {
        HttpHeaders::const_iterator header;

        if (countHeaderNameOccurrences(headers, "CONTENT-LENGTH") > 1
            && !isDuplicatesSameValue(headers, "CONTENT-LENGTH"))
        {
            _impl->parseResult.state = PARSE_ERROR;
            return false;
        }
        header = findHeader(headers, "CONTENT-LENGTH");
        if (header == headers.end())
            return true;
        if (header->value.empty())
        {
            _impl->parseResult.state = PARSE_ERROR;
            _impl->parseResult.httpStatus = 400;
            return false;
        }
        for (std::size_t i = 0; i < header->value.size(); i++)
        {
            if (!isdigit(static_cast<unsigned char>(header->value[i])))
            {
                _impl->parseResult.state = PARSE_ERROR;
                _impl->parseResult.httpStatus = 400;
                return false;
            }
        }

        
    }

    bool    HttpParser::isDuplicatesSameValue(const HttpHeaders& headers, const std::string& expected) {
        HttpHeaders::const_iterator it;
        std::string dup;
        bool    firstFound = false;

        for (it = headers.begin(); it != headers.end(); ++it)
        {
            if (isHeaderName(it->value, expected))
            {
                if (firstFound && dup != it->value)
                    return false;
                else if (!firstFound)
                {
                    dup = it->value;
                    firstFound = true;
                }
            }
        }
        return true;
    }

    HttpHeaders::const_iterator&    HttpParser::findHeader(const HttpHeaders& headers, const std::string& name) {
        HttpHeaders::const_iterator it;
        for (it = headers.begin(); it != headers.end(); ++it)
        {
            if (isHeaderName(name, "CONTENT-LENGTH"))
                return it;
        }
        return it;
    }

    const HttpRequest&  HttpParser::request() const {
        return _impl->httpRequest;
    }
}
