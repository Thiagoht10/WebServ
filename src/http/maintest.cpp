
#include "../../includes/ModuleInterfaces.hpp"
#include "../../includes/http/HttpParser.hpp"

#include <iostream>
#include <string>
#include <sstream>

using namespace webserv;

static void printResult(const ParseResult& result)
{
    if (result.state == PARSE_COMPLETE)
        std::cout << "\033[32mCOMPLETE\033[0m";
    else if (result.state == PARSE_NEED_MORE)
        std::cout << "\033[33mNEED_MORE\033[0m";
    else
        std::cout << "\033[31mERROR\033[0m";

    std::cout << " | HTTP: " << result.httpStatus
              << " | Consumed: " << result.consumed
              << " bytes";

    if (!result.detail.empty())
        std::cout << " | " << result.detail;

    std::cout << std::endl;
}

static void printRequest(const HttpRequest& request)
{
    HttpHeaders::const_iterator it;

    std::cout << "  Method: " << request.method << std::endl;
    std::cout << "  Target: " << request.target << std::endl;
    std::cout << "  Version: " << request.version << std::endl;

    std::cout << "  Headers (" << request.headers.size() << "):"
              << std::endl;

    for (it = request.headers.begin(); it != request.headers.end(); ++it)
        std::cout << "    " << it->name << ": "
                  << it->value << std::endl;

    std::cout << "  Body (" << request.body.size() << " bytes): ";

    if (request.body.empty())
        std::cout << "(empty)";
    else
        std::cout << "[" << request.body << "]";

    std::cout << std::endl;
}

static void printTestHeader(const std::string& name)
{
    std::cout << "\n========================================"
              << std::endl;
    std::cout << " TEST: " << name << std::endl;
    std::cout << "========================================"
              << std::endl;
}

// One complete request in a single feed().
static void testSingleFeed(
    const std::string& name,
    const std::string& raw,
    std::size_t maxBody,
    ParseState expectedState,
    int expectedStatus)
{
    HttpParser parser(maxBody);
    ParseResult result;

    printTestHeader(name);

    result = parser.feed(raw.data(), raw.size());

    printResult(result);

    if (result.state == PARSE_COMPLETE)
        printRequest(parser.request());

    if (result.state == expectedState
        && (expectedState != PARSE_ERROR
            || result.httpStatus == expectedStatus))
        std::cout << "\033[32m[PASS]\033[0m" << std::endl;
    else
        std::cout << "\033[31m[FAIL]\033[0m"
                  << " Expected state: " << expectedState
                  << " | Expected HTTP: " << expectedStatus
                  << std::endl;
}

// Simulate multiple recv() calls.
static void testMultipleFeeds(
    const std::string& name,
    const std::string& raw,
    std::size_t maxBody,
    std::size_t chunkSize,
    ParseState expectedState,
    int expectedStatus)
{
    HttpParser parser(maxBody);
    ParseResult result;
    std::size_t offset = 0;
    std::size_t currentSize;

    printTestHeader(name);

    while (offset < raw.size())
    {
        currentSize = chunkSize;

        if (offset + currentSize > raw.size())
            currentSize = raw.size() - offset;

        result = parser.feed(raw.data() + offset, currentSize);

        std::cout << "Feed #" << (offset / chunkSize + 1)
                  << " | Sent: " << currentSize << " bytes"
                  << " | ";

        printResult(result);

        offset += currentSize;

        if (result.state == PARSE_COMPLETE
            || result.state == PARSE_ERROR)
            break;
    }

    if (result.state == PARSE_COMPLETE)
        printRequest(parser.request());

    if (result.state == expectedState
        && (expectedState != PARSE_ERROR
            || result.httpStatus == expectedStatus))
        std::cout << "\033[32m[PASS]\033[0m" << std::endl;
    else
        std::cout << "\033[31m[FAIL]\033[0m"
                  << " Expected state: " << expectedState
                  << " | Expected HTTP: " << expectedStatus
                  << std::endl;
}

// Several requests received together in one buffer.
// Uses consumed to find the beginning of the next request.
static void testPipelining(
    const std::string& name,
    const std::string& raw,
    std::size_t maxBody,
    std::size_t expectedRequests)
{
    HttpParser parser(maxBody);
    ParseResult result;
    std::string remaining = raw;
    std::size_t requestCount = 0;
    std::size_t consumed;

    printTestHeader(name);

    while (!remaining.empty())
    {
        result = parser.feed(remaining.data(), remaining.size());

        std::cout << "\nRequest #" << (requestCount + 1) << std::endl;
        printResult(result);

        if (result.state != PARSE_COMPLETE)
        {
            std::cout << "\033[31m[FAIL]\033[0m"
                      << " Could not parse next request."
                      << std::endl;
            break;
        }

        printRequest(parser.request());

        consumed = result.consumed;

        if (consumed == 0 || consumed > remaining.size())
        {
            std::cout << "\033[31m[FAIL]\033[0m"
                      << " Invalid consumed value: " << consumed
                      << std::endl;
            break;
        }

        remaining.erase(0, consumed);
        requestCount++;

        if (!remaining.empty())
            parser.reset();
    }

    if (requestCount == expectedRequests && remaining.empty())
        std::cout << "\n\033[32m[PASS]\033[0m Parsed "
                  << requestCount << " requests."
                  << std::endl;
    else
        std::cout << "\n\033[31m[FAIL]\033[0m Expected "
                  << expectedRequests << " requests, parsed "
                  << requestCount << "."
                  << std::endl;
}

int main()
{
    // ========================================
    // 1. BASIC REQUESTS
    // ========================================

    testSingleFeed(
        "Simple GET",
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n",
        1024,
        PARSE_COMPLETE,
        0
    );

    testSingleFeed(
        "GET with multiple headers",
        "GET /index.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "User-Agent: TestClient/1.0\r\n"
        "Accept: text/html\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        1024,
        PARSE_COMPLETE,
        0
    );

    testSingleFeed(
        "HTTP 1.0 without Host",
        "GET / HTTP/1.0\r\n"
        "\r\n",
        1024,
        PARSE_COMPLETE,
        0
    );

    testSingleFeed(
        "POST with Content-Length",
        "POST /submit HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 11\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "Hello World",
        1024,
        PARSE_COMPLETE,
        0
    );

    // ========================================
    // 2. BODY TESTS
    // ========================================

    testSingleFeed(
        "Empty POST body",
        "POST /submit HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        1024,
        PARSE_COMPLETE,
        0
    );

    testSingleFeed(
        "Chunked body",
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n"
        "Hello\r\n"
        "6\r\n"
        " World\r\n"
        "0\r\n"
        "\r\n",
        1024,
        PARSE_COMPLETE,
        0
    );

    testSingleFeed(
        "Chunked body with extension",
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5;test=value\r\n"
        "Hello\r\n"
        "0\r\n"
        "\r\n",
        1024,
        PARSE_COMPLETE,
        0
    );

    // ========================================
    // 3. INVALID REQUESTS
    // ========================================

    testSingleFeed(
        "Unsupported HTTP method",
        "PUT / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n",
        1024,
        PARSE_ERROR,
        405
    );

    testSingleFeed(
        "Invalid HTTP version",
        "GET / HTTP/2.0\r\n"
        "Host: localhost\r\n"
        "\r\n",
        1024,
        PARSE_ERROR,
        505
    );

    testSingleFeed(
        "Missing Host in HTTP 1.1",
        "GET / HTTP/1.1\r\n"
        "\r\n",
        1024,
        PARSE_ERROR,
        400
    );

    testSingleFeed(
        "Duplicate Host",
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Host: example.com\r\n"
        "\r\n",
        1024,
        PARSE_ERROR,
        400
    );

    testSingleFeed(
        "Invalid header without colon",
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "InvalidHeader\r\n"
        "\r\n",
        1024,
        PARSE_ERROR,
        400
    );

    testSingleFeed(
        "Content-Length and Transfer-Encoding",
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n",
        1024,
        PARSE_ERROR,
        400
    );

    testSingleFeed(
        "Invalid Content-Length",
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: abc\r\n"
        "\r\n",
        1024,
        PARSE_ERROR,
        400
    );

    testSingleFeed(
        "Unsupported Transfer-Encoding",
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: gzip\r\n"
        "\r\n",
        1024,
        PARSE_ERROR,
        501
    );

    // ========================================
    // 4. BODY LIMIT TESTS
    // ========================================

    testSingleFeed(
        "Body exactly at limit",
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "12345",
        5,
        PARSE_COMPLETE,
        0
    );

    testSingleFeed(
        "Body exceeds limit",
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 6\r\n"
        "\r\n"
        "123456",
        5,
        PARSE_ERROR,
        413
    );

    testSingleFeed(
        "Chunked body exceeds limit",
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "6\r\n"
        "123456\r\n"
        "0\r\n"
        "\r\n",
        5,
        PARSE_ERROR,
        413
    );

    // ========================================
    // 5. MULTIPLE FEEDS
    // ========================================

    testMultipleFeeds(
        "GET received in small fragments",
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        1024,
        3,
        PARSE_COMPLETE,
        0
    );

    testMultipleFeeds(
        "POST body received in fragments",
        "POST /submit HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "Hello World",
        1024,
        7,
        PARSE_COMPLETE,
        0
    );

    testMultipleFeeds(
        "Chunked body received in fragments",
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n"
        "Hello\r\n"
        "0\r\n"
        "\r\n",
        1024,
        4,
        PARSE_COMPLETE,
        0
    );

    // ========================================
    // 6. MULTIPLE REQUESTS / PIPELINING
    // ========================================

    testPipelining(
        "Three GET requests in one feed",
        "GET /first HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n"
        "GET /second HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n"
        "GET /third HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n",
        1024,
        3
    );

    testPipelining(
        "Mixed GET and POST requests",
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n"
        "POST /submit HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "Hello"
        "GET /next HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n",
        1024,
        3
    );

    return 0;
}

