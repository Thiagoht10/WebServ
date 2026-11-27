
#include "../../includes/ModuleInterfaces.hpp"
#include "../../includes/http/HttpParser.hpp"

#include <iostream>
#include <string>
#include <sstream>

using namespace webserv;

static int g_passed = 0;
static int g_failed = 0;

static void printResult(const ParseResult& result)
{
    if (result.state == PARSE_COMPLETE)
        std::cout << "\033[32mCOMPLETE\033[0m";
    else if (result.state == PARSE_NEED_MORE)
        std::cout << "\033[33mNEED_MORE\033[0m";
    else
        std::cout << "\033[31mERROR\033[0m";

    std::cout << " | HTTP: " << result.httpStatus
              << " | Consumed: " << result.consumed << " bytes";

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

static void reportTest(bool passed, const std::string& details)
{
    if (passed)
    {
        ++g_passed;
        std::cout << "\033[32m[PASS]\033[0m " << details << std::endl;
    }
    else
    {
        ++g_failed;
        std::cout << "\033[31m[FAIL]\033[0m " << details << std::endl;
    }
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
    bool passed;

    printTestHeader(name);

    result = parser.feed(raw.data(), raw.size());

    printResult(result);

    if (result.state == PARSE_COMPLETE)
        printRequest(parser.request());

    passed = result.state == expectedState;

    if (expectedState == PARSE_ERROR)
        passed = passed && result.httpStatus == expectedStatus;

    if (expectedState == PARSE_COMPLETE)
        passed = passed && result.consumed == raw.size();

    reportTest(passed, "Expected state and consumed value");
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
    std::size_t totalConsumed = 0;
    bool passed = true;

    printTestHeader(name);

    while (offset < raw.size())
    {
        currentSize = chunkSize;

        if (offset + currentSize > raw.size())
            currentSize = raw.size() - offset;

        result = parser.feed(raw.data() + offset, currentSize);

        std::cout << "Feed #" << (offset / chunkSize + 1)
                  << " | Sent: " << currentSize << " bytes | ";

        printResult(result);

        if (result.consumed > currentSize)
            passed = false;

        totalConsumed += result.consumed;
        offset += currentSize;

        if (result.state == PARSE_COMPLETE
            || result.state == PARSE_ERROR)
            break;
    }

    if (result.state == PARSE_COMPLETE)
        printRequest(parser.request());

    passed = passed && result.state == expectedState;

    if (expectedState == PARSE_ERROR)
        passed = passed && result.httpStatus == expectedStatus;

    if (expectedState == PARSE_COMPLETE)
        passed = passed && totalConsumed == raw.size();

    reportTest(passed, "Fragmented request and consumed accounting");
}

// Several requests received together in one buffer.
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
    bool passed = true;

    printTestHeader(name);

    while (!remaining.empty())
    {
        result = parser.feed(remaining.data(), remaining.size());

        std::cout << "\nRequest #" << (requestCount + 1) << std::endl;
        printResult(result);

        if (result.state != PARSE_COMPLETE)
        {
            passed = false;
            break;
        }

        printRequest(parser.request());

        consumed = result.consumed;

        if (consumed == 0 || consumed > remaining.size())
        {
            passed = false;
            break;
        }

        remaining.erase(0, consumed);
        ++requestCount;

        if (!remaining.empty())
            parser.reset();
    }

    passed = passed && requestCount == expectedRequests
        && remaining.empty();

    std::ostringstream details;
    details << "Expected " << expectedRequests
            << " requests, parsed " << requestCount;

    reportTest(passed, details.str());
}

// Fragmented first request, followed by another request in the
// same final feed. Tests consumed relative to the latest feed.
static void testFragmentedPipelining(
    const std::string& name,
    const std::string& first,
    const std::string& second,
    std::size_t split,
    const std::string& secondTarget)
{
    HttpParser parser(1024);
    ParseResult result;
    std::string finalFeed;
    std::string remaining;
    std::size_t expectedConsumed;
    bool passed = true;

    printTestHeader(name);

    if (split == 0 || split >= first.size())
    {
        reportTest(false, "Invalid split position");
        return;
    }

    result = parser.feed(first.data(), split);

    std::cout << "Feed #1 | Sent: " << split << " bytes | ";
    printResult(result);

    if (result.state != PARSE_NEED_MORE
        || result.consumed != split)
        passed = false;

    finalFeed = first.substr(split) + second;

    result = parser.feed(finalFeed.data(), finalFeed.size());

    std::cout << "Feed #2 | Sent: " << finalFeed.size() << " bytes | ";
    printResult(result);

    expectedConsumed = first.size() - split;

    if (result.state != PARSE_COMPLETE
        || result.consumed != expectedConsumed)
        passed = false;
    else
        printRequest(parser.request());

    if (passed)
    {
        remaining = finalFeed.substr(result.consumed);

        if (remaining != second)
            passed = false;
    }

    if (passed)
    {
        parser.reset();

        result = parser.feed(remaining.data(), remaining.size());

        std::cout << "\nRemaining request | ";
        printResult(result);

        if (result.state != PARSE_COMPLETE
            || result.consumed != remaining.size()
            || parser.request().target != secondTarget)
            passed = false;
        else
            printRequest(parser.request());
    }

    reportTest(passed, "Fragmented pipelining and leftover bytes");
}

// Complete first request, incomplete second request, then finish it.
static void testIncompletePipelinedTail()
{
    HttpParser parser(1024);
    ParseResult result;

    std::string first =
        "GET /first HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    std::string partial =
        "GET /second HTTP/1.1\r\n"
        "Host: local";

    std::string ending = "host\r\n\r\n";
    std::string raw = first + partial;

    bool passed = true;

    printTestHeader("Complete request followed by incomplete request");

    result = parser.feed(raw.data(), raw.size());

    printResult(result);

    if (result.state != PARSE_COMPLETE
        || result.consumed != first.size())
        passed = false;
    else
        printRequest(parser.request());

    if (passed)
    {
        parser.reset();

        result = parser.feed(partial.data(), partial.size());

        std::cout << "\nSecond request, first part | ";
        printResult(result);

        if (result.state != PARSE_NEED_MORE
            || result.consumed != partial.size())
            passed = false;
    }

    if (passed)
    {
        result = parser.feed(ending.data(), ending.size());

        std::cout << "Second request, final part | ";
        printResult(result);

        if (result.state != PARSE_COMPLETE
            || result.consumed != ending.size()
            || parser.request().target != "/second")
            passed = false;
        else
            printRequest(parser.request());
    }

    reportTest(passed, "Incomplete pipelined request handling");
}

// Header limit tests.
static void testHeaderLimits()
{
    std::string raw;
    std::string largeValue(9000, 'a');
    std::ostringstream headerName;
    int i;

    raw =
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "X-Large: " + largeValue + "\r\n"
        "\r\n";

    testSingleFeed(
        "Individual header exceeds 8 KB",
        raw,
        1024,
        PARSE_ERROR,
        431
    );

    raw =
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n";

    for (i = 0; i < 100; ++i)
    {
        headerName.str("");
        headerName.clear();
        headerName << "X-Test-" << i;
        raw += headerName.str() + ": x\r\n";
    }

    raw += "\r\n";

    testSingleFeed(
        "Number of headers exceeds limit",
        raw,
        1024,
        PARSE_ERROR,
        431
    );

    raw =
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "X-First: " + std::string(3000, 'a') + "\r\n"
        "X-Second: " + std::string(3000, 'b') + "\r\n"
        "X-Third: " + std::string(3000, 'c') + "\r\n"
        "\r\n";

    testSingleFeed(
        "Total headers between 8 KB and 32 KB",
        raw,
        1024,
        PARSE_COMPLETE,
        0
    );
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
        1024, PARSE_COMPLETE, 0
    );

    testSingleFeed(
        "GET with multiple headers",
        "GET /index.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "User-Agent: TestClient/1.0\r\n"
        "Accept: text/html\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        1024, PARSE_COMPLETE, 0
    );

    testSingleFeed(
        "HTTP 1.0 without Host",
        "GET / HTTP/1.0\r\n"
        "\r\n",
        1024, PARSE_COMPLETE, 0
    );

    testSingleFeed(
        "POST with Content-Length",
        "POST /submit HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 11\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "Hello World",
        1024, PARSE_COMPLETE, 0
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
        1024, PARSE_COMPLETE, 0
    );

    testSingleFeed(
        "Chunked body",
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nHello\r\n"
        "6\r\n World\r\n"
        "0\r\n\r\n",
        1024, PARSE_COMPLETE, 0
    );

    testSingleFeed(
        "Chunked body with extension",
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5;test=value\r\n"
        "Hello\r\n"
        "0\r\n\r\n",
        1024, PARSE_COMPLETE, 0
    );

    testSingleFeed(
        "Content-Length body containing CRLF",
        "POST /submit HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
        "A\r\nB",
        1024, PARSE_COMPLETE, 0
    );

    testSingleFeed(
        "Duplicate identical Content-Length",
        "POST /submit HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "Hello",
        1024, PARSE_COMPLETE, 0
    );

    testSingleFeed(
        "Conflicting duplicate Content-Length",
        "POST /submit HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "Hello",
        1024, PARSE_ERROR, 400
    );

    testSingleFeed(
        "Case-insensitive headers and whitespace",
        "POST /submit HTTP/1.1\r\n"
        "hOsT:\t localhost \t\r\n"
        "cOnTeNt-LeNgTh: 4 \t\r\n"
        "X-Test:\t value \t\r\n"
        "\r\n"
        "A\r\nB",
        1024, PARSE_COMPLETE, 0
    );

    testSingleFeed(
        "Empty chunked body",
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "0\r\n\r\n",
        1024, PARSE_COMPLETE, 0
    );

    testSingleFeed(
        "Chunked body with trailers",
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nHello\r\n"
        "0\r\n"
        "X-Checksum: test\r\n"
        "\r\n",
        1024, PARSE_COMPLETE, 0
    );

    testSingleFeed(
        "Invalid hexadecimal chunk size",
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "XYZ\r\nHello\r\n0\r\n\r\n",
        1024, PARSE_ERROR, 400
    );

    testSingleFeed(
        "Missing CRLF after chunk data",
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nHelloXX0\r\n\r\n",
        1024, PARSE_ERROR, 400
    );

    testSingleFeed(
        "Mixed-case chunked Transfer-Encoding",
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: ChUnKeD\r\n"
        "\r\n"
        "5\r\nHello\r\n0\r\n\r\n",
        1024, PARSE_COMPLETE, 0
    );

    testSingleFeed(
        "Content-Length exceeding numeric range",
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 999999999999999999999999999999999999999999\r\n"
        "\r\n",
        1024, PARSE_ERROR, 413
    );

    // ========================================
    // 3. INVALID REQUESTS
    // ========================================

    testSingleFeed(
        "Unsupported HTTP method",
        "PUT / HTTP/1.1\r\nHost: localhost\r\n\r\n",
        1024, PARSE_ERROR, 405
    );

    testSingleFeed(
        "Invalid HTTP version",
        "GET / HTTP/2.0\r\nHost: localhost\r\n\r\n",
        1024, PARSE_ERROR, 505
    );

    testSingleFeed(
        "Missing Host in HTTP 1.1",
        "GET / HTTP/1.1\r\n\r\n",
        1024, PARSE_ERROR, 400
    );

    testSingleFeed(
        "Duplicate Host",
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Host: example.com\r\n\r\n",
        1024, PARSE_ERROR, 400
    );

    testSingleFeed(
        "Invalid header without colon",
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "InvalidHeader\r\n\r\n",
        1024, PARSE_ERROR, 400
    );

    testSingleFeed(
        "Content-Length and Transfer-Encoding",
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "Transfer-Encoding: chunked\r\n\r\n",
        1024, PARSE_ERROR, 400
    );

    testSingleFeed(
        "Invalid Content-Length",
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: abc\r\n\r\n",
        1024, PARSE_ERROR, 400
    );

    testSingleFeed(
        "Unsupported Transfer-Encoding",
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: gzip\r\n\r\n",
        1024, PARSE_ERROR, 501
    );

    // ========================================
    // 4. BODY LIMITS
    // ========================================

    testSingleFeed(
        "Body exactly at limit",
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n\r\n"
        "12345",
        5, PARSE_COMPLETE, 0
    );

    testSingleFeed(
        "Body exceeds limit",
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 6\r\n\r\n"
        "123456",
        5, PARSE_ERROR, 413
    );

    testSingleFeed(
        "Chunked body exceeds limit",
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n\r\n"
        "6\r\n123456\r\n0\r\n\r\n",
        5, PARSE_ERROR, 413
    );

    // ========================================
    // 5. REQUEST LINE LIMIT
    // ========================================

    testSingleFeed(
        "Request line exceeds 8 KB",
        "GET /" + std::string(9000, 'a')
        + " HTTP/1.1\r\nHost: localhost\r\n\r\n",
        1024, PARSE_ERROR, 414
    );

    // ========================================
    // 6. HEADER LIMITS
    // ========================================

    testHeaderLimits();

    // ========================================
    // 7. FRAGMENTED REQUESTS
    // ========================================

    testMultipleFeeds(
        "GET received in small fragments",
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: keep-alive\r\n\r\n",
        1024, 3, PARSE_COMPLETE, 0
    );

    testMultipleFeeds(
        "POST body received in fragments",
        "POST /submit HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 11\r\n\r\n"
        "Hello World",
        1024, 7, PARSE_COMPLETE, 0
    );

    testMultipleFeeds(
        "Chunked body received in fragments",
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n\r\n"
        "5\r\nHello\r\n0\r\n\r\n",
        1024, 4, PARSE_COMPLETE, 0
    );

    // ========================================
    // 8. PIPELINING
    // ========================================

    testPipelining(
        "Three GET requests in one feed",
        "GET /first HTTP/1.1\r\nHost: localhost\r\n\r\n"
        "GET /second HTTP/1.1\r\nHost: localhost\r\n\r\n"
        "GET /third HTTP/1.1\r\nHost: localhost\r\n\r\n",
        1024, 3
    );

    testPipelining(
        "Mixed GET and POST requests",
        "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
        "POST /submit HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n\r\n"
        "Hello"
        "GET /next HTTP/1.1\r\nHost: localhost\r\n\r\n",
        1024, 3
    );

    testPipelining(
        "POST followed by GET",
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n\r\n"
        "Hello"
        "GET /after HTTP/1.1\r\nHost: localhost\r\n\r\n",
        1024, 2
    );

    testPipelining(
        "Chunked POST followed by GET",
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n\r\n"
        "5\r\nHello\r\n0\r\n\r\n"
        "GET /after HTTP/1.1\r\nHost: localhost\r\n\r\n",
        1024, 2
    );

    testPipelining(
        "Chunked POST with multiple chunks followed by GET",
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n\r\n"
        "3\r\nABC\r\n"
        "3\r\nDEF\r\n"
        "0\r\n\r\n"
        "GET /after HTTP/1.1\r\nHost: localhost\r\n\r\n",
        1024, 2
    );

    {
        std::string first =
            "POST /upload HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Length: 5\r\n\r\n"
            "Hello";

        std::string second =
            "GET /after HTTP/1.1\r\n"
            "Host: localhost\r\n\r\n";

        testFragmentedPipelining(
            "Fragmented POST followed by GET",
            first, second, first.find("Hello") + 2, "/after"
        );
    }

    testIncompletePipelinedTail();

    // ========================================
    // 9. FINAL SUMMARY
    // ========================================

    std::cout << "\n========================================"
              << std::endl;
    std::cout << " TEST SUMMARY" << std::endl;
    std::cout << "========================================"
              << std::endl;

    std::cout << "\033[32mPassed: " << g_passed << "\033[0m"
              << std::endl;
    std::cout << "\033[31mFailed: " << g_failed << "\033[0m"
              << std::endl;

    std::cout << "Total: " << g_passed + g_failed << std::endl;

    return g_failed == 0 ? 0 : 1;
}
