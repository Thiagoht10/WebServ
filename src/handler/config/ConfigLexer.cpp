#include "ConfigLexer.hpp"

namespace
{
    bool    isWhiteSpace(const char c)
    {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            return true;

        return false;
    }
}

namespace webserv
{
    ConfigLexer::ConfigLexer() {}
    ConfigLexer::~ConfigLexer() {}

    std::vector<Token> ConfigLexer::tokenizer(const std::string& content) const
    {
        std::vector<Token>  tokens;
        std::string         current;
        size_t              line = 1;
        size_t              i = 0;

        while (i < content.size())
        {
            Token tk;

            if (content[i] == '#')
            {
                if (!current.empty())
                {
                    tk.line = line;
                    tk.value = current;
                    tokens.push_back(tk);
                    current.clear();
                }
                while (i < content.size() && content[i] != '\n')
                    i++;

                if (i == content.size())
                    break;
                
            }

            if (content[i] == '{' || content[i] == '}' || content[i] == ';')
            {
                if (!current.empty())
                {
                    tk.line = line;
                    tk.value = current;
                    tokens.push_back(tk);
                    current.clear();
                }

                tk.line = line;
                tk.value = content[i];
                tokens.push_back(tk);
            }

            else if (isWhiteSpace(content[i]))
            {
                if (!current.empty())
                {
                    tk.line = line;
                    tk.value = current;
                    tokens.push_back(tk);
                    current.clear();
                }

                if (content[i] == '\n')
                    line++;
            }

            else
            {
                current.append(1, content[i]);
            }

            i++;
        }

        if (!current.empty())
        {
            Token tk;

            tk.line = line;
            tk.value = current;
            tokens.push_back(tk);
            current.clear();
        }
        
        return tokens;
    }
}
