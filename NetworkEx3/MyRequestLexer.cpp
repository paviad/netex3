#include "MyRequest.h"

bool MyRequest::isseparator(char ch) {
    switch(ch) {
    case '(':
    case ')':
    case '<':
    case '>':
    case '@':
    case ',':
    case ';':
    case ':':
    case '\\':
    case '"':
    case '/':
    case '[':
    case ']':
    case '?':
    case '=':
    case '{':
    case '}':
        return true;
    }
    return false;
}

bool MyRequest::isctl(char ch) {
    if(ch <= 31 || ch == 127) return true;
    return false;
}

bool MyRequest::istext(char ch) {
    return !isctl(ch);
}

void MyRequest::Lexer(char *buf, int length) {
#ifdef USE_EXTERNAL_HTTP_PARSER
    http_parser_execute(&parser, &settings, buf, length);
    if(parser.http_errno != http_errno::HPE_OK) {
        http_errno e = (http_errno)parser.http_errno;
        urlValid = methodValid = httpVersionValid = true;
        if(e == http_errno::HPE_INVALID_URL) urlValid = false;
        if(e == http_errno::HPE_INVALID_METHOD) methodValid = false;
        if(e == http_errno::HPE_INVALID_VERSION) httpVersionValid = false;
        aborted = true;
    }
#else
    if(length == 0) {
        valid = methodValid && urlValid && httpVersionValid;
        if(data.size() == contentLength)
            done = true;
        return;
    }
    if(parserState == PARSER_body) {
        AppendData(buf, length);
        return;
    }
    for(int i = 0; i < length; i++) {
        if(parserState == PARSER_body) {
            AppendData(buf + i, length - i);
            break;
        }
        char ch = buf[i];
        rawHeader.push_back(ch);
again:
        if(parserState == PARSER_body) {
            AppendData(buf + i, length - i);
            break;
        }
        if(lexerState > LEXER_error) {
            aborted = true;
            return;
        }
        switch(lexerState) {
        case LEXER_init:
            if((isctl(ch) || isseparator(ch) || ch == ' ' || ch == '\t') && !lexerToken.empty()) {
                ProcessTokenIfAvailable();
                goto again;
            }
            if(ch == '\r') lexerState = LEXER_cr_read;
            else if(ch == '\n') ProcessLF();
            else if(opaqueEnabled) {
                lexerOpaqueString.clear();
                lexerState = LEXER_opaque;
                goto again;
            }
            else if(parseURLEnabled && !isspace(ch)) {
                lexerUrl.clear();
                lexerState = LEXER_url;
                goto again;
            }
            else if(ch == '"' && quotedStringsEnabled) {
                lexerQuotedString.clear();
                lexerState = LEXER_quoted_string;
            }
            else if(ch == '(' && commentsEnabled) {
                lexerComment.clear();
                lexerComment.push_back(ch);
                lexerState = LEXER_comment;
                lexerCommentNesting = 1;
            }
            else if(ch == ' ' || ch == '\t') lexerState = LEXER_LWS_read;
            else if(isseparator(ch)) ProcessSeparator(ch);
            else if(!isctl(ch)) lexerToken.push_back(ch);
            else lexerState = LEXER_token_text_error;
            break;
        case LEXER_opaque:
            ProcessTokenIfAvailable();
            if(ch == '\r') {
                if(foldingEnabled) lexerState = LEXER_opaque_cr_read;
                else {
                    ProcessOpaque(lexerOpaqueString);
                    lexerState = LEXER_init;
                    goto again;
                }
            }
            else lexerOpaqueString.push_back(ch);
            break;
        case LEXER_opaque_cr_read:
            ProcessTokenIfAvailable();
            if(ch == '\n') lexerState = LEXER_opaque_newline_read;
            else {
                ProcessOpaque(lexerOpaqueString);
                lexerState = LEXER_init;
                goto again;
            }
            break;
        case LEXER_opaque_newline_read:
            if(ch == ' ' || ch == '\t') {
                lexerOpaqueString.push_back(' ');
                lexerState = LEXER_opaque;
            }
            else {
                ProcessOpaque(lexerOpaqueString);
                ProcessNewline();
                lexerState = LEXER_init;
                goto again;
            }
            break;
        case LEXER_url:
            ProcessTokenIfAvailable();
            if(isspace(ch)) {
                ProcessURL(lexerUrl);
                lexerState = LEXER_init;
                goto again;
            }
            else {
                if(ch == '%') lexerState = LEXER_url_encoded_1;
                else if(ch == '+') lexerUrl.push_back(' ');
                else lexerUrl.push_back(ch);
            }
            break;
        case LEXER_url_encoded_1:
            if(ch == '%') {
                lexerUrl.push_back('%');
                lexerState = LEXER_url;
            }
            else if(isxdigit(ch)) {
                lexerTmpHex[0] = ch;
                lexerTmpHex[2] = '\0';
                lexerState = LEXER_url_encoded_2;
            }
            else lexerState = LEXER_url_error;
            break;
        case LEXER_url_encoded_2:
            if(isxdigit(ch)) {
                lexerTmpHex[1] = ch;
                char newch = strtol(lexerTmpHex, NULL, 16);
                lexerUrl.push_back(newch);
                lexerState = LEXER_url;
            }
            else lexerState = LEXER_url_error;
            break;
        case LEXER_comment:
            ProcessTokenIfAvailable();
            if(ch == '\\') lexerState = LEXER_quoted_pair_comment;
            else if(ch == '(') {
                lexerCommentNesting++;
                lexerComment.push_back(ch);
            }
            else if(ch == ')') {
                lexerCommentNesting--;
                lexerComment.push_back(ch);
                if(lexerCommentNesting == 0) {
                    ProcessComment(lexerComment);
                    lexerState = LEXER_init;
                }
            }
            else if(istext(ch)) {
                lexerComment.push_back(ch);
            }
            else lexerState = LEXER_comment_text_error;
            break;
        case LEXER_quoted_pair_comment:
            ProcessTokenIfAvailable();
            lexerComment.push_back(ch);
            lexerState = LEXER_comment;
            break;
        case LEXER_quoted_string:
            ProcessTokenIfAvailable();
            if(ch == '\\') lexerState = LEXER_quoted_pair;
            else if(ch == '"') {
                ProcessQuotedString(lexerQuotedString);
                lexerState = LEXER_init;
            }
            else if(istext(ch)) {
                lexerQuotedString.push_back(ch);
            }
            else lexerState = LEXER_quoted_string_text_error;
            break;
        case LEXER_quoted_pair:
            ProcessTokenIfAvailable();
            lexerQuotedString.push_back(ch);
            lexerState = LEXER_quoted_string;
            break;
        case LEXER_cr_read:
            ProcessTokenIfAvailable();
            if(ch == '\n') {
                if(foldingEnabled) lexerState = LEXER_newline_read;
                else {
                    ProcessNewline();
                    lexerState = LEXER_init;
                }
            }
            else {
                ProcessCR();
                lexerState = LEXER_init;
                goto again;
            }
            break;
        case LEXER_newline_read: // LWS folding state
            ProcessTokenIfAvailable();
            if(ch == ' ' || ch == '\t') lexerState = LEXER_LWS_read;
            else {
                ProcessNewline();
                lexerState = LEXER_init;
                goto again;
            }
            break;
        case LEXER_LWS_read: // partial LWS
            ProcessTokenIfAvailable();
            if(ch == '\r') lexerState = LEXER_LWS_cr_read;
            else if(ch != ' ' && ch != '\t') {
                lexerState = LEXER_init;
                ProcessLWS();
                goto again;
            }
            break;
        case LEXER_LWS_cr_read:
            ProcessTokenIfAvailable();
            if(ch == '\n') {
                if(foldingEnabled) lexerState = LEXER_LWS_newline_read;
                else {
                    ProcessLWS();
                    ProcessNewline();
                    lexerState = LEXER_init;
                }
            }
            else {
                ProcessCR();
                lexerState = LEXER_init;
                goto again;
            }
            break;
        case LEXER_LWS_newline_read:
            ProcessTokenIfAvailable();
            if(ch == ' ' || ch == '\t') lexerState = LEXER_LWS_read;
            else {
                ProcessLWS();
                ProcessNewline();
                lexerState = LEXER_init;
                goto again;
            }
            break;
        }
    }
    if(parserState == PARSER_body) {
        valid = methodValid && urlValid && httpVersionValid;
        if(data.size() == contentLength)
            done = true;
    }
#endif
}
