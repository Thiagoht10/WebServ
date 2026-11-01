#ifndef CONFIG_LEXER_HPP
#define CONFIG_LEXER_HPP

#include <string>
#include <vector>

namespace webserv
{
    struct Token
    {
        std::string value;
        size_t      line;
    };
    
    class ConfigLexer
    {
    public:
        ConfigLexer();
        ~ConfigLexer();
        std::vector<Token>  tokenizer(const std::string& content) const;
    };

} // namespace webserv


#endif