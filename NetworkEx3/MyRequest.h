#pragma once

#include <string>
#include <cctype>
#include <unordered_set>
#ifdef USE_EXTERNAL_HTTP_PARSER
#include "..\http-parser\http_parser.h"
#endif
#include "trim.h"

using namespace std;

class MyRequest {
private:
    int state;

    static unordered_set<string> allowedMethods;
    static unordered_set<string> commentedHeaders;
    static char invalidPathChars[];

    static class __initializer {
    public:
        __initializer();
    };
    static __initializer __initializerInstance;

#ifdef USE_EXTERNAL_HTTP_PARSER
    http_parser_settings settings;
    http_parser parser;
#endif

public:
    bool methodValid;
    string method;
    bool urlValid;
    string url;
    bool httpVersionValid;
    string httpVersion;
    bool valid;
    bool aborted;
    bool done;
    vector<char> data;
    int contentLength;
    bool keepAlive;
    string rawHeader;
    int socketId;

    string tmpHeader;
    string tmpHeaderValue;

#ifdef USE_EXTERNAL_HTTP_PARSER
private:
    friend int on_url_received(http_parser *parser, const char *at, size_t length);
    friend int on_body_received(http_parser *parser, const char *at, size_t length);
    friend int on_header_field_received(http_parser *parser, const char *at, size_t length);
    friend int on_header_value_received(http_parser *parser, const char *at, size_t length);
    friend int on_message_complete(http_parser *parser);

    void OnURLReceived(http_parser *parser, const char *at, size_t length);
    void OnBodyReceived(http_parser *parser, const char *at, size_t length);
    void OnHeaderFieldReceived(http_parser *parser, const char *at, size_t length);
    void OnHeaderValueReceived(http_parser *parser, const char *at, size_t length);
    void OnMessageComplete(http_parser *parser);
#endif

private:
    void CheckMethod() {
        methodValid = allowedMethods.find(method) != allowedMethods.end();
    }

    void CheckURL(bool decode = false) {
        if(decode) {
            // First, decode the URL by handling "%xx" (xx = two digit hex number), and "+" occurrences.
            string newurl;
            char hex[3] = { '\0' };
            int tmp = 0;
            for(string::iterator it = url.begin(); it != url.end(); ++it) {
                if(tmp == 0 && *it == '+') newurl.push_back(' ');
                else if(tmp == 0 && *it != '%') newurl.push_back(*it);
                else if(tmp == 0 && *it == '%') tmp = 1;
                else if(tmp == 1 && isxdigit(*it)) {
                    hex[0] = *it;
                    tmp = 2;
                }
                else if(tmp == 1 && *it == '%') {
                    newurl.push_back('%');
                    tmp = 0;
                }
                else if(tmp == 2 && isxdigit(*it)) {
                    hex[1] = *it;
                    char newch = strtol(hex, NULL, 16);
                    newurl.push_back(newch);
                    tmp = 0;
                }
                else return;
            }
            if(tmp == 0) {
                url = newurl;
            }
        }
        if(url[0] == '/') urlValid = true;
    }

    void CheckHTTPVersion() {
        // We only support HTTP/1.1
        if(httpVersion != "HTTP/1.1") return;
        httpVersionValid = true;
    }
private:
    bool commentsEnabled;
    bool quotedStringsEnabled;
    bool parseURLEnabled;
    bool foldingEnabled;
    bool opaqueEnabled;

    enum {
        LEXER_init,
        LEXER_cr_read,
        LEXER_newline_read,
        LEXER_LWS_read,
        LEXER_LWS_cr_read,
        LEXER_LWS_newline_read,
        LEXER_quoted_string,
        LEXER_quoted_pair,
        LEXER_comment,
        LEXER_quoted_pair_comment,
        LEXER_url,
        LEXER_url_encoded_1,
        LEXER_url_encoded_2,
        LEXER_opaque,
        LEXER_opaque_cr_read,
        LEXER_opaque_newline_read,

        LEXER_error,

        LEXER_quoted_string_text_error,
        LEXER_comment_text_error,
        LEXER_token_text_error,
        LEXER_url_error,
    } lexerState;

    string lexerQuotedString;
    string lexerComment;
    int lexerCommentNesting;
    string lexerToken;
    string lexerUrl;
    string lexerOpaqueString;
    char lexerTmpHex[3];

    enum TOKEN {
        TOK_token,
        TOK_separator,
        TOK_newline,
        TOK_cr,
        TOK_lf,
        TOK_lws,
        TOK_qstring,
        TOK_comment,
        TOK_url,
        TOK_opaque
    };

    enum {
        PARSER_init, // initial state
        PARSER_url, // after reading method, before reading url
        PARSER_version, // after reading url, before reading http version
        PARSER_version_slash, // after reading the HTTP part of the http version, before reading the '/'
        PARSER_version_major_minor, // after reading the '/' part of the http version, before reading the number
        PARSER_version_ok, // after reading the http version completely, expecting LWS or newline.
        PARSER_header, // before header, expect a token, or a newline signifying the end of the request header.
        PARSER_header_colon, // after eading header field, before reading the ':' separator
        PARSER_header_value, // after reading the ':' separator, before reading a field's opaque value
        PARSER_header_commented_value, // after reading the ':' separator, before reading a field's commented value
        PARSER_header_commented_separator, // after reading a separator in a commented field value
        PARSER_header_done, // after reading a complete header field and value
        PARSER_body,

        PARSER_error,

        PARSER_method_error,
        PARSER_url_error,
        PARSER_version_error,
        PARSER_bad_request_error,
        PARSER_header_error,
    } parserState;

    string parserElement;
public:
    MyRequest() {
        Reset();
    }

    void Reset() {
        state = 0;
        contentLength = 0;
        keepAlive = false;
        done = aborted = httpVersionValid = methodValid = urlValid = valid = false;

        method.clear();
        url.clear();
        httpVersion.clear();
        tmpHeaderValue.clear();

        commentsEnabled = quotedStringsEnabled = parseURLEnabled = foldingEnabled = opaqueEnabled = false;
        lexerState = LEXER_init;
        lexerCommentNesting = 0;
        lexerTmpHex[2] = '\0';

        parserState = PARSER_init;
#ifdef USE_EXTERNAL_HTTP_PARSER
        http_parser_init(&parser, http_parser_type::HTTP_REQUEST);
        parser.data = (void*)this;
        memset(&settings, 0, sizeof(settings));
        settings.on_url = on_url_received;
        settings.on_message_complete = on_message_complete;
        settings.on_body = on_body_received;
        settings.on_header_field = on_header_field_received;
        settings.on_header_value = on_header_value_received;
#endif
    }

    int AppendData(char *buf, int length) {
        int newLength = length;
        if(data.size() + newLength > contentLength)
            newLength = contentLength - data.size();
        data.insert(data.end(), buf, buf + newLength);
        if(data.size() == contentLength)
            done = true;
        return newLength;
    }

    int ParseChunk(char *buf, int length) {
#ifdef USE_EXTERNAL_HTTP_PARSER
        int consumed = http_parser_execute(&parser, &settings, buf, length);
        if(parser.http_errno != http_errno::HPE_OK) {
            http_errno e = (http_errno)parser.http_errno;
            urlValid = methodValid = httpVersionValid = true;
            if(e == http_errno::HPE_INVALID_URL) urlValid = false;
            if(e == http_errno::HPE_INVALID_METHOD) methodValid = false;
            if(e == http_errno::HPE_INVALID_VERSION) httpVersionValid = false;
            aborted = true;
        }
        return consumed;
#else
        return Lexer(buf, length);
#endif
    }

    void ProcessHeader(string &hdr, string &value);

    bool isseparator(char ch);
    bool isctl(char ch);
    bool istext(char ch);

    int Lexer(char *buf, int length);

    bool IsHeaderOpaque(string &hdr);
    void ProcessTokenIfAvailable();
    void ProcessToken(string &str);
    void ProcessSeparator(char ch);
    void ProcessNewline();
    void ProcessCR();
    void ProcessLF();
    void ProcessLWS();
    void ProcessQuotedString(string &str);
    void ProcessComment(string &str);
    void ProcessURL(string &str);
    void ProcessOpaque(string &str);

    void Parse(TOKEN tok);
};
