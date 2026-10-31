#ifndef MODULE_INTERFACES_HPP
# define MODULE_INTERFACES_HPP

# include <cstddef>
# include <string>
# include <vector>
# include <sys/types.h>

/*
 * Contrato de comunicacao entre os tres modulos do mesmo programa.
 * 1: sockets, poll(), transferencias e ciclo de vida dos descritores.
 * 2: interpretacao das requisicoes e serializacao das respostas HTTP.
 * 3: configuracao, rotas, operacoes solicitadas e execucao de CGI.
 *
 * Fluxo normal: 1 recebe bytes -> 2 interpreta -> 3 processa ->
 * 2 monta a resposta -> 1 envia os bytes ao cliente.
 * Fluxo CGI: 3 inicia o processo -> 1 acompanha os pipes ->
 * 3 interpreta a saida -> 2 monta a resposta -> 1 envia.
 *
 * As chamadas sao locais: nao existe uma ligacao de rede entre modulos.
 * Este header declara contratos; nenhuma funcao esta implementada aqui.
 * As structs sem construtor exigem inicializacao explicita dos campos
 * escalares (int, bool, enum, pid_t e size_t) por quem as produzir.
 */
namespace webserv
{
    // Definida em ServerConfig.hpp; incluir esse header nos .cpp que
    // consultarem os campos da configuracao.
    class ServerConfig;

    // Um par nome/valor, sem os dois-pontos nem o CRLF final.
    // Exemplo: name = "Content-Type", value = "text/html".
    struct HttpHeader
    {
        std::string name;
        std::string value;
    };

    // Usa uma sequencia para preservar campos repetidos; nao combina
    // automaticamente headers que tenham o mesmo nome.
    typedef std::vector<HttpHeader> HttpHeaders;

    // Produzida pelo parser do participante 2 e lida pelo participante 3.
    // method: "GET", "POST", "DELETE", etc.
    // target: alvo original, por exemplo "/search?q=abc" (inclui a query).
    // version: por exemplo "HTTP/1.1"; headers: campos ja separados.
    // body pode conter bytes nulos: usar size(), nao strlen().
    // keepAlive expressa a persistencia pedida/permitida pela requisicao;
    // a resposta ainda pode determinar que a ligacao seja fechada.
    struct HttpRequest
    {
        std::string method;
        std::string target;
        std::string version;
        HttpHeaders headers;
        std::string body;       // Corpo ja descodificado, incluindo chunked.
        bool        keepAlive;
    };

    // Resposta do participante 2 para o participante 1:
    // NEED_MORE: conservar o parser e entregar novos bytes quando chegarem.
    // COMPLETE: request() ja pode ser consultado e encaminhado ao handler.
    // ERROR: parar esta analise e preparar uma resposta HTTP de erro.
    enum ParseState
    {
        PARSE_NEED_MORE,
        PARSE_COMPLETE,
        PARSE_ERROR
    };

    // consumed permite separar esta requisicao de bytes posteriores.
    // Se feed() receber 200 bytes e consumir 150, os ultimos 50 continuam
    // a pertencer ao participante 1; nao devem ser descartados.
    // Nunca reenviar bytes que o parser ja declarou consumidos.
    struct ParseResult
    {
        ParseState  state;
        std::size_t consumed;  // Bytes consumidos APENAS nesta chamada.
        int         httpStatus; // 0 sem erro; codigo HTTP em PARSE_ERROR.
        std::string detail;    // Diagnostico interno.
    };

    // PARTICIPANTE 2. Uma instancia por cliente.
    // feed() copia/retem o necessario; nunca guarda o ponteiro recebido.
    // Nao faz I/O nem espera por novos bytes.
    class HttpParser
    {
    public:
        // Limite do corpo descodificado, incluindo pedidos chunked.
        // Zero permite apenas corpos vazios.
        explicit HttpParser(std::size_t maxBodyBytes);
        ~HttpParser();

        // data aponta para size bytes validos, sem exigir terminador nulo.
        // Pode receber apenas parte de um header ou parte do corpo.
        // Conserva o progresso internamente e retorna sem aguardar rede.
        // Devolve PARSE_ERROR/413 assim que identificar excesso do limite,
        // sem acumular o corpo inteiro antes de verificar o seu tamanho.
        ParseResult feed(const char* data, std::size_t size);
        // Chamar quando recv() retornar 0: o cliente terminou a entrada.
        // Permite distinguir fim de entrada de uma requisicao completa;
        // EOF nao transforma uma requisicao truncada numa requisicao valida.
        // Nao significa simplesmente "nao ha dados disponiveis agora".
        ParseResult endOfInput();
        const HttpRequest& request() const; // Apenas em PARSE_COMPLETE.
        // A referencia devolvida por request() pertence ao parser e so
        // deve ser usada ate reset() ou destruicao. Copiar se necessario.
        // reset() prepara a proxima requisicao, preservando maxBodyBytes;
        // nao fecha o socket.
        void reset();

    private:
        HttpParser(const HttpParser&);
        HttpParser& operator=(const HttpParser&);
        // Pimpl: os dados internos serao definidos no .cpp.
        // As operacoes de copia privadas impedem copias acidentais em C++98.
        struct Impl;
        Impl* _impl;
    };

    // Resultado sem a status line e sem os delimitadores HTTP.
    // Headers repetidos sao permitidos, por exemplo Set-Cookie.
    // O participante 3 fornece o resultado sem o serializar em HTTP.
    // statusCode: por exemplo 200, 404 ou 500.
    // headers: metadados do resultado; body: conteudo, inclusive binario.
    // closeConnection pede o fecho depois de enviar a resposta inteira.
    struct ResponseData
    {
        int         statusCode;
        HttpHeaders headers;
        std::string body;
        bool        closeConnection;
    };

    // Identifica um processo CGI e os extremos dos pipes usados pelo pai.
    // pid identifica o processo filho; os FDs sao numeros do processo pai.
    // inputFd NAO e o extremo que o filho usa para ler.
    // Copiar esta struct copia numeros, nao duplica FDs nem transfere posse.
    // A posse e transferida uma unica vez ao participante 1 no arranque.
    struct CgiJob
    {
        pid_t pid;
        int   inputFd;  // Extremo do pai: escrita no stdin do CGI.
        int   outputFd; // Extremo do pai: leitura do stdout do CGI.
    };

    // READY: existe resultado completo para construir a resposta HTTP.
    // CGI_PENDING: processo iniciado, mas ainda falta recolher o resultado.
    // ERROR: existe resposta de erro pronta em response; nao e uma excecao.
    enum HandlerState
    {
        HANDLER_READY,
        HANDLER_CGI_PENDING,
        HANDLER_ERROR
    };

    // Ler primeiro state e depois apenas os campos validos nesse estado.
    // Em ERROR, response.statusCode identifica o erro HTTP a responder.
    // detail serve para logs; nao deve ser exposto automaticamente ao cliente.
    struct HandlerResult
    {
        HandlerState state;
        ResponseData response; // Valida em READY e ERROR.
        CgiJob       cgi;      // Valido apenas em CGI_PENDING.
        std::string  detail;   // Diagnostico interno.
    };

    // Falhas notificadas pela infraestrutura ao processamento do pedido.
    // IO_ERROR: falha efetiva num pipe; TIMEOUT: prazo excedido;
    // CANCELLED: trabalho abandonado, por exemplo por desconexao do cliente.
    enum CgiFailure
    {
        CGI_IO_ERROR,
        CGI_TIMEOUT,
        CGI_CANCELLED
    };

    // PARTICIPANTE 3. Cria pipes, fork, dup2 e execve.
    // Em sucesso, transfere ao participante 1 a posse dos dois FDs do pai,
    // ja nao bloqueantes. Fecha todos os extremos desnecessarios.
    // Em falha, limpa recursos e devolve HANDLER_ERROR.
    class CGIHandler
    {
    public:
        CGIHandler();
        ~CGIHandler();

        // Configura ambiente, argumentos e redirecionamentos do filho.
        // Retorna PENDING em sucesso de arranque ou ERROR em falha.
        // Nao espera pela conclusao do CGI. Falhas posteriores de execucao
        // tambem precisam de ser tratadas pelo acompanhamento do filho.
        HandlerResult start(const HttpRequest& request,
                            const ServerConfig& config);

    private:
        CGIHandler(const CGIHandler&);
        CGIHandler& operator=(const CGIHandler&);
        // Pimpl: os dados internos serao definidos no .cpp.
        // As operacoes de copia privadas impedem copias acidentais em C++98.
        struct Impl;
        Impl* _impl;
    };

    // PARTICIPANTE 3. Processa uma requisicao de cada vez; pode ser
    // reutilizado na mesma ligacao apos reset().
    // handle() copia os dados de que necessitar apos retornar.
    // Pode devolver resposta imediata, erro HTTP ou trabalho CGI pendente.
    class RequestHandler
    {
    public:
        RequestHandler();
        ~RequestHandler();

        // Recebe uma requisicao completa e aplica configuracao/rota.
        // Para CGI, devolve os descritores criados por CGIHandler::start().
        // Nao espera pelo processo CGI; o event loop continua a trabalhar.
        HandlerResult handle(const HttpRequest& request,
                             const ServerConfig& config);

        // Prepara uma nova requisicao depois da conclusao da anterior e
        // da limpeza/recolha do seu CGI pelo participante 1.
        // Nao fecha descritores nem recolhe processos.
        void reset();

        // Nao exige HttpRequest valida nem altera o processamento em curso.
        // Usa a pagina configurada; se ausente/ilegivel, usa pagina padrao.
        // Inicializa todos os campos, com closeConnection = true;
        // o chamador pode ajustar a persistencia quando for seguro.
        ResponseData makeError(int statusCode,
                               const ServerConfig& config) const;

        // Chamadas pelo participante 1 apos os eventos correspondentes.
        // Recebem bytes ja lidos; nao leem os pipes por conta propria.
        // Entrega exatamente size bytes lidos pelo participante 1.
        // O handler conserva o necessario e interpreta headers/corpo CGI.
        void onCgiOutput(const char* data, std::size_t size);
        // Notifica fim real da saida: read() retornou 0 apos drenar o pipe.
        // POLLHUP pode coexistir com bytes por ler; recolhe-los antes do EOF.
        void onCgiOutputEof();
        // Chamar depois de waitpid() devolver o pid do filho, nunca quando
        // WNOHANG devolver 0. O filho pode sair com bytes ainda no pipe:
        // terminacao do processo e EOF da saida sao eventos diferentes.
        void onCgiExit(int waitStatus); // Status bruto de waitpid().
        // Regista a falha para produzir o resultado apropriado.
        // Esta notificacao nao transfere a posse dos FDs para o handler.
        void onCgiFailure(CgiFailure reason);

        // Nao bloqueia. Continua PENDING enquanto faltar saida/terminacao.
        // Em READY/ERROR, cgi nao e valido: nao registar os FDs novamente.
        // "pollResult" consulta o estado do handler; nao chama poll().
        // Pode ser consultado depois das notificacoes acima. Um resultado
        // ERROR tambem pode ficar pronto antes da terminacao do filho;
        // a limpeza/recolha do processo continua a cargo do participante 1.
        HandlerResult pollResult() const;

    private:
        RequestHandler(const RequestHandler&);
        RequestHandler& operator=(const RequestHandler&);
        // Pimpl: os dados internos serao definidos no .cpp.
        // As operacoes de copia privadas impedem copias acidentais em C++98.
        struct Impl;
        Impl* _impl;
    };

    // Entrega final do participante 2 ao participante 1.
    // bytes inclui status line, headers, linha vazia e corpo permitido.
    // closeAfterSend so e aplicado depois de enviar todos os bytes.
    struct SerializedResponse
    {
        std::string bytes; // Resposta HTTP completa, pronta para send().
        bool        closeAfterSend;
    };

    // PARTICIPANTE 2. Monta status line, headers e corpo.
    // Define o enquadramento HTTP e normaliza Content-Length/Connection.
    class HttpResponseBuilder
    {
    public:
        // Usa tambem a requisicao para decidir persistencia e regras HTTP.
        // Nao envia nada: devolve os bytes completos para o buffer de saida.
        SerializedResponse build(const HttpRequest& request,
                                 const ResponseData& response) const;
        // Permite responder mesmo se o parser nao conseguiu construir uma
        // HttpRequest valida. O codigo fornecido deve ser um erro HTTP.
        SerializedResponse buildError(int statusCode,
                                      bool closeConnection) const;
        // Erro com conteudo escolhido por makeError(), sem HttpRequest
        // valida. Respeita closeConnection e normaliza o enquadramento HTTP.
        SerializedResponse buildError(const ResponseData& response) const;
    };

    // Estado de uma ligacao, controlado pelo participante 1:
    // READING_REQUEST: receber e alimentar o parser.
    // PROCESSING_REQUEST: encaminhar a requisicao completa ao handler.
    // WAITING_CGI: acompanhar pipes e processo sem parar os outros clientes.
    // WRITING_RESPONSE: enviar outgoing.bytes a partir de bytesSent.
    // CLOSING: remover eventos e libertar recursos da ligacao.
    enum ClientState
    {
        CLIENT_READING_REQUEST,
        CLIENT_PROCESSING_REQUEST,
        CLIENT_WAITING_CGI,
        CLIENT_WRITING_RESPONSE,
        CLIENT_CLOSING
    };

    // PARTICIPANTE 1. Uma instancia por ligacao; inicializar via construtor.
    // Mantem bytes nao consumidos pelo parser para a proxima requisicao.
    // Nesta interface, processa uma requisicao de cada vez por ligacao.
    class ClientContext
    {
    public:
        // Assume a posse do socket fornecido. Inicializa estado, FDs e
        // contadores; a implementacao do destrutor deve libertar recursos.
        // Guarda a configuracao do listener e inicializa parser com
        // config.maxBodyBytes. A configuracao deve sobreviver ao cliente.
        ClientContext(int socketFd, const ServerConfig& config);
        ~ClientContext();

        // Ponteiro nao proprietario, sempre valido; nao apagar nem substituir.
        const ServerConfig* config;
        // Ligacao TCP com o cliente; nao confundir com os pipes do CGI.
        int                socketFd;
        ClientState        state;
        HttpParser         parser;
        RequestHandler     handler;
        // Bytes recebidos que feed() ainda nao consumiu, incluindo eventual
        // inicio da requisicao seguinte. Remover apenas consumed bytes.
        std::string        unreadBytes;
        // Buffer mantido vivo durante todos os envios parciais.
        SerializedResponse outgoing;
        // send() deve usar bytes + bytesSent e size - bytesSent.
        // Incrementar apenas pelo numero positivo devolvido por send().
        // Ao atingir outgoing.bytes.size(), a resposta foi toda enviada.
        std::size_t        bytesSent;

        CgiJob             cgi;
        // Copia do corpo descodificado a enviar ao stdin do CGI.
        // Mesmo um corpo vazio exige fechar inputFd para sinalizar EOF.
        std::string        cgiInput;
        // Progresso independente de bytesSent: refere-se ao pipe do CGI,
        // enquanto bytesSent refere-se a resposta enviada ao cliente TCP.
        std::size_t        cgiBytesSent;

        // Monitorizar escrita apenas enquanto existirem bytes pendentes;
        // monitorizar leitura da saida CGI enquanto outputFd estiver ativo.
        // A prontidao do poll nao garante transferencia completa: guardar
        // sempre o progresso e regressar ao event loop quando necessario.
        // Para keep-alive, apos o envio preparar novo estado de requisicao,
        // incluindo parser/handler, preservando os bytes ainda nao consumidos.
        // Participante 1 possui/fecha socket e FDs CGI recebidos.
        // FDs inativos: -1; pid inativo: -1; contadores iniciais: 0.
        // Remove FDs do poll antes de fechar e marca-os como -1.
        // Fecha inputFd ao terminar o corpo para sinalizar EOF ao CGI.
        // Recolhe o filho com waitpid(WNOHANG), sem bloquear o event loop.
        // Em cancelamento/timeout/desconexao, termina e recolhe o filho.
        // O participante 1 controla prazos de leitura, CGI e envio.
        // Timeout CGI e notificado por onCgiFailure(CGI_TIMEOUT).
        // Sockets (incluindo listeners) e pipes usam o mesmo poll().
        // Leitura/escrita exige prontidao indicada previamente pelo poll;
        // nao consultar errno para decidir comportamento apos essas operacoes.
        // Arquivos regulares dispensam notificacoes de prontidao.

    private:
        ClientContext(const ClientContext&);
        ClientContext& operator=(const ClientContext&);
    };
}

#endif
