#ifndef SERVER_CONFIG_HPP
# define SERVER_CONFIG_HPP

# include <cstddef>
# include <map>
# include <string>
# include <vector>

namespace webserv
{
    struct ListenAddress
    {
        std::string    host;
        unsigned short port;

        ListenAddress() : port(0) {}
    };

    struct RouteConfig
    {
        std::string              path;
        std::string              root;
        std::vector<std::string> allowedMethods;
        bool                     directoryListing;
        std::string              indexFile;
        bool                     uploadEnabled;
        std::string              uploadDirectory;
        int                      redirectStatus; // 0: sem redirecionamento.
        std::string              redirectTarget;

        // Extensao -> executavel CGI; exemplo: ".py" -> "/usr/bin/python3".
        std::map<std::string, std::string> cgiInterpreters;

        RouteConfig()
            : directoryListing(false), uploadEnabled(false),
              redirectStatus(0)
        {
        }
    };

    // Produzida pelo participante 3; consultada pelos participantes 1 e 3.
    // Cada listener identifica a configuracao dos clientes que aceitar.
    // Carregar antes de abrir sockets; manter a colecao viva e sem alteracoes
    // enquanto listeners/clientes guardarem ponteiros para os seus elementos.
    class ServerConfig
    {
    public:
        std::vector<ListenAddress> listeners;
        std::size_t               maxBodyBytes;
        std::map<int, std::string> errorPages; // Codigo HTTP -> caminho.
        std::vector<RouteConfig>  routes;

        // Padrao do projeto: 1 MiB. Zero permite apenas corpos vazios.
        ServerConfig() : maxBodyBytes(1024 * 1024) {}
    };

    // PARTICIPANTE 3. Implementacao no .cpp do modulo de configuracao.
    class ConfigLoader
    {
    public:
        // Devolve configuracoes validadas, com defaults ja aplicados.
        // Rejeita listeners ambiguos entre servidores (sem virtual hosts).
        // Em erro de leitura/validacao, lanca std::runtime_error, tratado
        // no arranque antes da abertura dos sockets; nao devolve dados parciais.
        std::vector<ServerConfig> load(const std::string& path) const;
    };
}

#endif
