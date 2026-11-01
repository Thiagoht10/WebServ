#ifndef TOKEN_STREAM_HPP
#define TOKEN_STREAM_HPP

#include <vector>
#include "ConfigLexer.hpp"

namespace webserv
{
    class TokenStream
    {
    private:
        const std::vector<Token>& _tokens;
        size_t  _position;

    public:
        TokenStream(const std::vector<Token>& tokens);
        ~TokenStream();

        const Token&    peek() const;
        Token   consume();
        void    expect(const std::string& value);
        bool    atEnd() const;

    };
}



#endif