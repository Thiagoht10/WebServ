# include "../../includes/ModuleInterfaces.hpp"
# include <iostream>
# include <string>
# include <sstream>
# include <algorithm>

namespace webserv
{
    struct HttpParser::Impl
    {
        enum parseStage
        {
            REQUEST_LINE,
            HEADERS,
            BODY,
            COMPLETE,
        };

        enum chunkedBody
        {
            HEX_LINE,
            DATA_LINE,
            TRAILER_LINE,
        };

        parseStage  parseStage;
        chunkedBody chunkedStage;

        HttpRequest httpRequest;
        ParseResult parseResult;

        std::string data;
        std::size_t dataPrevSize;
        std::size_t feedSize;
        std::size_t chunkedSize;

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

        std::size_t bodySizeCount;

        bool    httpVersion;
        bool    chunkedInitialized;

    };

    HttpParser::HttpParser(std::size_t maxBodyBytes) : _maxBodyBytes(maxBodyBytes), 
        _impl(new Impl()) {
        _impl->parseStage = Impl::REQUEST_LINE;
        _impl->chunkedStage = Impl::HEX_LINE;
        _impl->httpVersion = false;
        _impl->chunkedInitialized = false;
        _impl->feedSize = 0;
        _impl->dataPrevSize = 0;
        _impl->headerStart = 0;
        _impl->headerEnd = 0;
        _impl->bodyStart = 0;
        _impl->requestStart = 0;
        _impl->requestEnd = 0;
        _impl->lineStart = 0;
        _impl->lineEnd = 0;
        _impl->lineLength = 0;
        _impl->chunkedSize = 0;
        _impl->bodySizeCount = 0;
    }
    
    HttpParser::~HttpParser() {
        delete _impl;
    }

    ParseResult HttpParser::feed(const char* data, std::size_t size) {
        _impl->dataPrevSize = _impl->data.size();
        _impl->feedSize = size;

        _impl->parseResult.consumed = 0;
        _impl->data.append(data, size);
        if (_impl->parseStage == Impl::REQUEST_LINE)
        {
            if (!parseRequestLine())
                return _impl->parseResult;
            _impl->parseResult.consumed = calculateConsumed(_impl->requestEnd + 2);
            _impl->parseStage = Impl::HEADERS;
        }

        if (_impl->parseStage == Impl::HEADERS)
        {
            if (!parseHeaders())
                return _impl->parseResult;
            _impl->bodyStart;
        }

        if (_impl->parseStage == Impl::BODY)
        {

        }


    }

    bool    HttpParser::parseRequestLine() {
        // extrai a request line
        _impl->requestEnd = _impl->data.find("\r\n", _impl->requestStart);
        if (_impl->requestEnd == std::string::npos)
        {
            _impl->parseResult.consumed = _impl->feedSize;
            return (fillParseResult(PARSE_NEED_MORE, 0, NULL, false));
        }
        _impl->requestLine = _impl->data.substr(_impl->requestStart , _impl->requestEnd - _impl->requestStart);
        if (_impl->requestLine.find('\t') != std::string::npos)
            return (fillParseResult(PARSE_ERROR, 400, NULL, false));
    
        // comeca por encontrar o primeiro espaco para poder separar em seguida q request line em method, target e version
        _impl->lineEnd = _impl->requestLine.find(" ");
        if (_impl->lineEnd == std::string::npos || _impl->lineEnd == 0)
            return (fillParseResult(PARSE_ERROR, 400, NULL, false));

        _impl->lineLength = _impl->lineEnd - _impl->lineStart;
        _impl->httpRequest.method = _impl->requestLine.substr(_impl->lineStart, _impl->lineLength);

        _impl->lineStart = _impl->lineEnd + 1;
        _impl->lineEnd = _impl->requestLine.find(" ", _impl->lineStart);

        if (_impl->lineEnd == std::string::npos || _impl->lineEnd == _impl->lineStart)
            return (fillParseResult(PARSE_ERROR, 400, NULL, false));

        _impl->lineLength = _impl->lineEnd - _impl->lineStart;
        _impl->httpRequest.target = _impl->requestLine.substr(_impl->lineStart, _impl->lineLength);

        _impl->lineStart = _impl->lineEnd + 1;
        if (_impl->lineStart >= _impl->requestLine.size())
            return (fillParseResult(PARSE_ERROR, 400, NULL, false));

        _impl->httpRequest.version = _impl->requestLine.substr(_impl->lineStart);
        if (_impl->httpRequest.version.find(' ') != std::string::npos)
            return (fillParseResult(PARSE_ERROR, 400, NULL, false));

        if (_impl->httpRequest.method != "GET" && _impl->httpRequest.method != "POST" 
            && _impl->httpRequest.method != "DELETE")
            return (fillParseResult(PARSE_ERROR, 405, NULL, false));

        if (!isValidHttpVersion(_impl->httpRequest.version))
            return false;
        return true;
    }

    bool    HttpParser::parseHeaders() {
        HttpHeader header;

        _impl->headerEnd = _impl->data.find("\r\n\r\n");
        if (_impl->headerEnd == std::string::npos)
        {
            _impl->parseResult.consumed = _impl->feedSize;
            return (fillParseResult(PARSE_NEED_MORE, 0, NULL, false));
        }
        _impl->bodyStart = _impl->headerEnd + 4;

        _impl->headerStart = _impl->requestEnd + 2;
        _impl->lineStart = _impl->headerStart;
        while (_impl->headerStart < _impl->headerEnd)
        {
            //extrai um header
            _impl->lineEnd = _impl->data.find("\r\n", _impl->lineStart);
            if (_impl->lineEnd == std::string::npos || _impl->lineEnd > _impl->headerEnd)
                return (fillParseResult(PARSE_ERROR, 400, NULL, false));
            _impl->lineLength = _impl->lineEnd - _impl->lineStart;
            _impl->headerLine = _impl->data.substr(_impl->lineStart, _impl->lineLength);

            // divide o header em name e value
            _impl->headerSeparator = _impl->headerLine.find(':');
            if (_impl->headerSeparator == std::string::npos || _impl->headerSeparator == 0)
                return (fillParseResult(PARSE_ERROR, 400, NULL, false));
            header.name = _impl->headerLine.substr(0, _impl->headerSeparator);

            header.value = _impl->headerLine.substr(_impl->headerSeparator + 1);
            _impl->httpRequest.headers.push_back(header);

            _impl->lineStart = _impl->lineEnd + 2;
            _impl->headerStart = _impl->lineEnd + 2;
        }
        if (!validateHeaders(_impl->httpRequest.headers))
            return false;
        return true;
    }

    bool    HttpParser::validateHeaders(HttpHeaders&    headers) {
        HttpHeaders::iterator it;
        std::size_t hostCount = 0;
        std::size_t first;
        std::size_t last;

        for (it = headers.begin(); it != headers.end(); ++it)
        {
            if (it->name.empty() || isOnlyWhiteSpaces(it->name))
                return (fillParseResult(PARSE_ERROR, 400, NULL, false));

            // Da um trim no value de qualquer header se for preciso removendo trailing spaces e leading spaces
            if (it->name.find(' ') != std::string::npos || it->name.find('\t') != std::string::npos)
                return (fillParseResult(PARSE_ERROR, 400, NULL, false));

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
                return (fillParseResult(PARSE_ERROR, 400, NULL, false));
            if (!isValidHeaderName(it->name))
                return (fillParseResult(PARSE_ERROR, 400, NULL, false));
        }

        // verifica se existe um "HOST" na versao 1.1 e se existe mais de 1 header de "HOST"
        hostCount = countHeaderNameOccurrences(headers, "HOST");
        if (_impl->httpVersion && hostCount != 1)
            return (fillParseResult(PARSE_ERROR, 400, NULL, false));
        else if (!_impl->httpVersion && hostCount > 1)
            return (fillParseResult(PARSE_ERROR, 400, NULL, false));

        if (!isValidTransferEncodingHeader(headers))
            return false;

        if (!isValidContentLengthHeader(headers))
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
            return false;
        return true;
    }

    bool    HttpParser::parseBody() {
        HttpHeaders::const_iterator header;

        if (countHeaderNameOccurrences(_impl->httpRequest.headers, "TRANSFER-ENCODING") == 1)
        {
            if (!parseBodyTransferEncoding())
                return false;
        }
        else if (countHeaderNameOccurrences(_impl->httpRequest.headers, "CONTENT-LENGTH"))
        {
            
        }
        _impl->parseStage = Impl::COMPLETE;
        return true;
    }

    bool    HttpParser::parseBodyTransferEncoding() {
        std::size_t dataEnd;
        std::size_t extensionPos = 0;
        std::string hex;

        if (!_impl->chunkedInitialized)
        {
            _impl->lineStart = _impl->bodyStart;
            _impl->chunkedInitialized = true;
        }
        while (true)
        {
            if (_impl->chunkedStage == Impl::HEX_LINE)
            {
                _impl->lineEnd = _impl->data.find("\r\n", _impl->lineStart);
                if (_impl->lineEnd == std::string::npos)
                {
                    _impl->parseResult.consumed = _impl->feedSize;
                    return (fillParseResult(PARSE_NEED_MORE, 0, NULL, false));
                }
                hex = _impl->data.substr(_impl->lineStart, _impl->lineEnd - _impl->lineStart);
                extensionPos = hex.find(';');
                if (extensionPos != std::string::npos)
                    hex = hex.substr(0, extensionPos);

                if (hex.empty())
                    return (fillParseResult(PARSE_ERROR, 400, NULL, false));

                for (std::size_t i = 0; i < hex.size(); i++)
                {
                    if (!std::isxdigit(static_cast<unsigned char>(hex[i])))
                        return (fillParseResult(PARSE_ERROR, 400, NULL, false));
                }

                std::stringstream ss(hex);
                ss >> std::hex >> _impl->chunkedSize;
                if (ss.fail() || !ss.eof())
                    return (fillParseResult(PARSE_ERROR, 400, NULL, false));

                if (_impl->chunkedSize > _maxBodyBytes - _impl->bodySizeCount)
                    return (fillParseResult(PARSE_ERROR, 413, NULL, false));
            
                _impl->bodySizeCount += _impl->chunkedSize;
                _impl->lineStart = _impl->lineEnd + 2;
                if (_impl->chunkedSize == 0)
                {
                    _impl->chunkedStage = Impl::TRAILER_LINE;
                    continue ;
                }
                _impl->chunkedStage = Impl::DATA_LINE;
            }

            if (_impl->bodySizeCount > _maxBodyBytes)
                return (fillParseResult(PARSE_ERROR, 413, NULL, false));

            if (_impl->chunkedStage == Impl::TRAILER_LINE)
            {
                _impl->lineEnd = _impl->data.find("\r\n", _impl->lineStart);
                if (_impl->lineEnd == std::string::npos)
                {
                    _impl->parseResult.consumed = _impl->feedSize;
                    return (fillParseResult(PARSE_NEED_MORE, 0, NULL, false));
                }

                if (_impl->lineEnd == _impl->lineStart)
                {
                    _impl->lineStart += 2;
                    _impl->chunkedInitialized = false;
                    _impl->chunkedStage = Impl::HEX_LINE;
                    _impl->parseResult.consumed = calculateConsumed(_impl->lineStart);
                    return true;
                }
                _impl->lineStart = _impl->lineEnd + 2;
            }

            if (_impl->chunkedStage == Impl::DATA_LINE)
            {
                if (_impl->data.size() - _impl->lineStart < _impl->chunkedSize + 2)
                {
                    _impl->parseResult.consumed = _impl->feedSize;
                    return (fillParseResult(PARSE_NEED_MORE, 0, NULL, false));
                }
                _impl->httpRequest.body.append(_impl->data, _impl->lineStart, _impl->chunkedSize);

                dataEnd = _impl->lineStart + _impl->chunkedSize;
                if (_impl->data[dataEnd] != '\r' || _impl->data[dataEnd + 1] != '\n')
                    return (fillParseResult(PARSE_ERROR, 400, NULL, false));

                _impl->lineStart += _impl->chunkedSize + 2;
                _impl->chunkedStage = Impl::HEX_LINE;
            }
        }
    }

    bool    HttpParser::compareTwoStringsCaseInsensitive(std::string str, const std::string& expected) const {
        for (std::size_t i = 0; i < str.size(); i++)
            str[i] = std::toupper(static_cast<unsigned char>(str[i]));
        return (str == expected);
    }

    bool    HttpParser::isValidHttpVersion(const std::string& version) {
        if (version.length() != 8)
            return (fillParseResult(PARSE_ERROR, 400, NULL, false));
        if (version.compare(0, 5, "HTTP/") != 0)
            return (fillParseResult(PARSE_ERROR, 400, NULL, false));

        if (!isdigit(static_cast<unsigned char>(version[5]))
            || version[6] != '.'
            || !isdigit(static_cast<unsigned char>(version[7])))
            return (fillParseResult(PARSE_ERROR, 400, NULL, false));
            
        if (version == "HTTP/1.1")
            _impl->httpVersion = true;
        else if (version != "HTTP/1.0")
            return (fillParseResult(PARSE_ERROR, 505, NULL, false));

        return true;
    }

    bool    HttpParser::isValidTransferEncodingHeader(const HttpHeaders&  headers) {
        bool    chunkedLast = false;
        bool    chunkedFound = false;
        HttpHeaders::const_iterator it;
        std::size_t start;
        std::size_t end;

        // verificar se existe um header de "Transfer-Encoding" de acordo com a versao HTTP
        if (countHeaderNameOccurrences(headers, "TRANSFER-ENCODING") != 0 
            && !_impl->httpVersion)
            return (fillParseResult(PARSE_ERROR, 400, NULL, false));
        // verifica se existe "Transfer-Encoding" e "Content-Length" na mesma request
        if (countHeaderNameOccurrences(headers, "TRANSFER-ENCODING") != 0 
            && countHeaderNameOccurrences(headers, "CONTENT-LENGTH") != 0)
            return (fillParseResult(PARSE_ERROR, 400, NULL, false));
        
        it = findHeader(headers, "TRANSFER-ENCODING");
        if (it == headers.end())
            return true;
        else
        {
            while (it != headers.end())
            {
                start = 0;
                if (!isHeaderName(it->name, "TRANSFER-ENCODING"))
                {
                    ++it;
                    continue ;
                }
                while (start < it->value.size())
                {
                    end = it->value.find(',', start);
                    if (end == std::string::npos)
                        end = it->value.size();
                    std::string token = it->value.substr(start, end - start);
                    token = trimStr(token);
                    if (token.empty())
                        return (fillParseResult(PARSE_ERROR, 400, NULL, false));
                    if (compareTwoStringsCaseInsensitive(token, "CHUNKED") && !chunkedFound)
                        chunkedFound = true;
                    else if (compareTwoStringsCaseInsensitive(token, "CHUNKED") && chunkedFound)
                        return (fillParseResult(PARSE_ERROR, 400, NULL, false));

                    if (!isSupportedTransferEncoding(token) && !chunkedFound)
                        return (fillParseResult(PARSE_ERROR, 501, NULL, false));
                    else if (!isSupportedTransferEncoding(token) && chunkedFound)
                        return (fillParseResult(PARSE_ERROR, 400, NULL, false));

                    if (compareTwoStringsCaseInsensitive(token, "CHUNKED"))
                    {
                        if (end != it->value.size())
                            return (fillParseResult(PARSE_ERROR, 400, NULL, false));
                        chunkedLast = true;
                    }
                    start = end + 1;
                }
                ++it;
            }
            if (!chunkedFound)
                return (fillParseResult(PARSE_ERROR, 501, NULL, false));
            if (!chunkedLast)
                return (fillParseResult(PARSE_ERROR, 400, NULL, false));
        }
        return true;
    }

    bool    HttpParser::isValidContentLengthHeader(const HttpHeaders&    headers) {
        HttpHeaders::const_iterator header;

        if (countHeaderNameOccurrences(headers, "CONTENT-LENGTH") > 1
            && !isDuplicatesSameValue(headers, "CONTENT-LENGTH"))
            return (fillParseResult(PARSE_ERROR, 400, NULL, false));
        header = findHeader(headers, "CONTENT-LENGTH");
        if (header == headers.end())
            return true;
        if (header->value.empty())
            return (fillParseResult(PARSE_ERROR, 400, NULL, false));
        for (std::size_t i = 0; i < header->value.size(); i++)
        {
            if (!isdigit(static_cast<unsigned char>(header->value[i])))
                return (fillParseResult(PARSE_ERROR, 400, NULL, false));
        }
        return true;        
    }

    bool    HttpParser::isDuplicatesSameValue(const HttpHeaders& headers, const std::string& expected) {
        HttpHeaders::const_iterator it;
        std::string dup;
        bool    firstFound = false;

        for (it = headers.begin(); it != headers.end(); ++it)
        {
            if (isHeaderName(it->name, expected))
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

    HttpHeaders::const_iterator    HttpParser::findHeader(const HttpHeaders& headers, const std::string& expected) {
        HttpHeaders::const_iterator it;
        for (it = headers.begin(); it != headers.end(); ++it)
        {
            if (isHeaderName(it->name, expected))
                return it;
        }
        return it;
    }

    bool    HttpParser::fillParseResult(ParseState state, int httpStatus, std::string details, bool tf) {
        _impl->parseResult.state = state;
        _impl->parseResult.httpStatus = httpStatus;
        _impl->parseResult.detail = details;
        return tf;
    }

    bool    HttpParser::isOnlyWhiteSpaces(const std::string& val) const {
        return val.find_first_not_of(" \t") == std::string::npos;
    }

    bool    HttpParser::isSupportedTransferEncoding(const std::string& token) const {
        if (compareTwoStringsCaseInsensitive(token, "GZIP")
            || compareTwoStringsCaseInsensitive(token, "COMPRESS")
            || compareTwoStringsCaseInsensitive(token, "DEFLATE")
            || compareTwoStringsCaseInsensitive(token, "BR")
            || compareTwoStringsCaseInsensitive(token, "ZSTD"))
            return false;
        else if (compareTwoStringsCaseInsensitive(token, "CHUNKED"))
            return true;
        return false;
    }

    std::string HttpParser::trimStr(const std::string& str) {
        std::size_t first = str.find_first_not_of(" \t");
        if (first == std::string::npos)
            return "";
        std::size_t last = str.find_last_not_of(" \t");

        return str.substr(first, last - first + 1);
    }

    std::size_t HttpParser::calculateConsumed(std::size_t absolutePosistion) const {
        if (absolutePosistion <= _impl->feedSize)
            return 0;
        return (std::min(absolutePosistion - _impl->dataPrevSize, _impl->feedSize));
    }

    const HttpRequest&  HttpParser::request() const {
        return _impl->httpRequest;
    }
}
