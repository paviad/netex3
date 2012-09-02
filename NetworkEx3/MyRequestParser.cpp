#include "MyRequest.h"

bool MyRequest::IsHeaderOpaque(string &hdr) {
    return commentedHeaders.find(hdr) == commentedHeaders.end();
}

void MyRequest::ProcessTokenIfAvailable() {
    if(lexerToken.empty()) return;
    ProcessToken(lexerToken);
    lexerToken.clear();
}

void MyRequest::ProcessToken(string &str) {
    parserElement = str;
    Parse(TOK_token);
}

void MyRequest::ProcessSeparator(char ch) {
    parserElement = string(1, ch);
    Parse(TOK_separator);
}

void MyRequest::ProcessNewline() {
    Parse(TOK_newline);
}

void MyRequest::ProcessCR() {
    Parse(TOK_cr);
}

void MyRequest::ProcessLF() {
    Parse(TOK_lf);
}

void MyRequest::ProcessLWS() {
    Parse(TOK_lws);
}

void MyRequest::ProcessQuotedString(string &str) {
    parserElement = str;
    Parse(TOK_qstring);
}

void MyRequest::ProcessComment(string &str) {
    parserElement = str;
    Parse(TOK_comment);
}

void MyRequest::ProcessURL(string &str) {
    parserElement = str;
    Parse(TOK_url);
}

void MyRequest::ProcessOpaque(string &str) {
    parserElement = str;
    Parse(TOK_opaque);
}

void MyRequest::Parse(TOKEN tok) {
    if(parserState > PARSER_error) {
        aborted = true;
        return;
    }
    switch(parserState) {
    case PARSER_init:
        if(tok == TOK_token) {
            method = parserElement;
            CheckMethod();
            if(!methodValid) parserState = PARSER_method_error;
            else {
                parseURLEnabled = true;
                parserState = PARSER_url;
            }
        }
        else parserState = PARSER_method_error;
        break;
    case PARSER_url:
        if(tok == TOK_url) {
            parseURLEnabled = false;
            url = parserElement;
            CheckURL();
            if(!urlValid) parserState = PARSER_url_error;
            else parserState = PARSER_version;
        }
        else if(tok != TOK_lws) parserState = PARSER_url_error;
        break;
    case PARSER_version:
        if(tok == TOK_token) {
            httpVersion.append(parserElement);
            parserState = PARSER_version_slash;
        }
        else if(tok != TOK_lws) parserState = PARSER_version_error;
        break;
    case PARSER_version_slash:
        if(tok == TOK_separator) {
            httpVersion.append(parserElement);
            parserState = PARSER_version_major_minor;
        }
        else parserState = PARSER_version_error;
        break;
    case PARSER_version_major_minor:
        if(tok == TOK_token) {
            httpVersion.append(parserElement);
            CheckHTTPVersion();
            if(httpVersionValid) parserState = PARSER_version_ok;
            else parserState = PARSER_version_error;
        }
        else parserState = PARSER_version_error;
        break;
    case PARSER_version_ok:
        if(tok == TOK_newline) parserState = PARSER_header;
        else if(tok != TOK_lws) parserState = PARSER_bad_request_error;
        break;
    case PARSER_header:
        if(tok == TOK_token) {
            tmpHeader = parserElement;
            parserState = PARSER_header_colon;
        }
        else if(tok == TOK_newline) {
            parserState = PARSER_body;
        }
        else parserState = PARSER_header_error;
        break;
    case PARSER_header_colon:
        if(tok == TOK_separator && parserElement == ":") {
            if(IsHeaderOpaque(tmpHeader)) {
                opaqueEnabled = true;
                parserState = PARSER_header_value;
            }
            else {
                quotedStringsEnabled = true;
                commentsEnabled = true;
                parserState = PARSER_header_commented_value;
                tmpHeaderValue.clear();
            }
            foldingEnabled = true;
        }
        else parserState = PARSER_header_error;
        break;
    case PARSER_header_value:
        if(tok == TOK_opaque) {
            foldingEnabled = false;
            opaqueEnabled = false;
            commentsEnabled = false;
            quotedStringsEnabled = false;
            tmpHeaderValue = parserElement;
            parserState = PARSER_header_done;
        }
        else parserState = PARSER_header_error;
        break;
    case PARSER_header_commented_value:
    case PARSER_header_commented_separator:
        if(tok == TOK_token) {
            tmpHeaderValue.append(parserElement);
        }
        else if(tok == TOK_separator) {
            tmpHeaderValue = trim(tmpHeaderValue);
            tmpHeaderValue.append(parserElement);
            parserState = PARSER_header_commented_separator;
        }
        else if(tok == TOK_qstring) {
            tmpHeaderValue.append(parserElement);
        }
        else if(tok == TOK_lws) 
        {
            if(parserState == PARSER_header_commented_separator)
                parserState = PARSER_header_commented_value;
            else
                tmpHeaderValue.push_back(' ');
        }
        else if(tok == TOK_comment)
            ;
        else if(tok == TOK_newline) 
        {
            foldingEnabled = false;
            opaqueEnabled = false;
            commentsEnabled = false;
            quotedStringsEnabled = false;
            ProcessHeader(tmpHeader, trim(tmpHeaderValue));
            parserState = PARSER_header;
        }
        else parserState = PARSER_header_error;
        break;
    case PARSER_header_done:
        if(tok == TOK_newline) {
            ProcessHeader(tmpHeader, trim(tmpHeaderValue));
            parserState = PARSER_header;
        }
        break;
    }
}
