#include "TokenStream.hpp"
#include <stdexcept>
#include <sstream>

namespace webserv
{
    TokenStream::TokenStream(const std::vector<Token>& tokens)
        : _tokens(tokens), _position(0)
    {}

    TokenStream::~TokenStream() {}

    const Token& TokenStream::peek() const
    {
        if (atEnd())
            throw std::runtime_error("unexpected end of configuration");

        return _tokens[_position];
    }

    Token TokenStream::consume()
    {
        if (atEnd())
            throw std::runtime_error("unexpected end of configuration");

        Token tk;

        tk.line = _tokens[_position].line;
        tk.value = _tokens[_position].value;

        _position++;

        return tk;
    }

    void    TokenStream::expect(const std::string& value)
    {
        std::ostringstream error;
        
        if (atEnd())
        {
            error << "unexpected end of configuration: expected \""
                  << value << "\"";
            throw std::runtime_error(error.str());
        }

        if (value != _tokens[_position].value)
        {
            error << "line " << _tokens[_position].line << ": ";
            error << "expected \"" << value << "\"";

            throw std::runtime_error(error.str());
        }
        else
            _position++;
    }

    bool    TokenStream::atEnd() const
    {
        if (_position >= _tokens.size())
            return true;

        return false;
    }
}