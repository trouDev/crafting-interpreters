#include <ctype.h>
#include <string.h>
#include "scanner.h"
static const char *start;
static const char *current;
static int line;
void initScanner(const char *source)
{
    start = current = source;
    line = 1;
}
static bool atEnd() { return *current == '\0'; }
static char advance() { return *current++; }
static char peek() { return atEnd() ? '\0' : *current; }
static char peekNext() { return atEnd() ? '\0' : current[1]; }
static bool match(char c)
{
    if (peek() != c)
        return false;
    current++;
    return true;
}
static Token make(TokenType t)
{
    Token x = {t, start, (int)(current - start), line};
    return x;
}
static Token error(const char *m)
{
    Token x = {TOKEN_ERROR, m, (int)strlen(m), line};
    return x;
}
static void skip()
{
    for (;;)
    {
        char c = peek();
        if (c == ' ' || c == '\r' || c == '\t')
            advance();
        else if (c == '\n')
        {
            line++;
            advance();
        }
        else if (c == '/' && peekNext() == '/')
        {
            while (peek() != '\n' && !atEnd())
                advance();
        }
        else
            break;
    }
}
static TokenType keyword(int n, const char *s, TokenType t)
{
    return current - start == n + 1 &&
                   memcmp(start + 1, s, n) == 0
               ? t
               : TOKEN_IDENTIFIER;
}
static TokenType idType()
{
    switch (start[0])
    {
    case 'a':
        return keyword(2, "nd", TOKEN_AND);
    case 'c':
        return keyword(4, "lass", TOKEN_CLASS);
    case 'e':
        return keyword(3, "lse", TOKEN_ELSE);
    case 'i':
        return keyword(1, "f", TOKEN_IF);
    case 'n':
        return keyword(2, "il", TOKEN_NIL);
    case 'o':
        return keyword(1, "r", TOKEN_OR);
    case 'p':
        return keyword(4, "rint", TOKEN_PRINT);
    case 'r':
        return keyword(5, "eturn", TOKEN_RETURN);
    case 's':
        return keyword(4, "uper", TOKEN_SUPER);
    case 'v':
        return keyword(2, "ar", TOKEN_VAR);
    case 'w':
        return keyword(4, "hile", TOKEN_WHILE);
    case 'f':
        if (current - start > 1)
        {
            if (start[1] == 'a')
                return keyword(4, "alse", TOKEN_FALSE);
            if (start[1] == 'o')
                return keyword(3, "or", TOKEN_FOR);
            if (start[1] == 'u')
                return keyword(3, "un", TOKEN_FUN);
        }
        break;
    case 't':
        if (current - start > 1)
        {
            if (start[1] == 'h')
                return keyword(3, "his", TOKEN_THIS);
            if (start[1] == 'r')
                return keyword(3, "rue", TOKEN_TRUE);
        }
        break;
    }
    return TOKEN_IDENTIFIER;
}
static Token identifier()
{
    while (isalnum(peek()) || peek() == '_')
        advance();
    return make(idType());
}
static Token number()
{
    while (isdigit(peek()))
        advance();
    if (peek() == '.' && isdigit(peekNext()))
    {
        advance();
        while (isdigit(peek()))
            advance();
    }
    return make(TOKEN_NUMBER);
}
static Token string()
{
    while (peek() != '"' && !atEnd())
    {
        if (peek() == '\n')
            line++;
        advance();
    }
    if (atEnd())
        return error("Unterminated string.");
    advance();
    return make(TOKEN_STRING);
}
Token scanToken()
{
    skip();
    start = current;
    if (atEnd())
        return make(TOKEN_EOF);
    char c = advance();
    if (isalpha(c) || c == '_')
        return identifier();
    if (isdigit(c))
        return number();
    if (c == '"')
        return string();
    switch (c)
    {
    case '(':
        return make(TOKEN_LEFT_PAREN);
    case ')':
        return make(TOKEN_RIGHT_PAREN);
    case '{':
        return make(TOKEN_LEFT_BRACE);
    case '}':
        return make(TOKEN_RIGHT_BRACE);
    case ',':
        return make(TOKEN_COMMA);
    case '.':
        return make(TOKEN_DOT);
    case '-':
        return make(TOKEN_MINUS);
    case '+':
        return make(TOKEN_PLUS);
    case ';':
        return make(TOKEN_SEMICOLON);
    case '?':
        return make(TOKEN_QUESTION);
    case ':':
        return make(TOKEN_COLON);
    case '/':
        return make(TOKEN_SLASH);
    case '*':
        return make(TOKEN_STAR);
    case '!':
        return make(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=':
        return make(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<':
        return make(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>':
        return make(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    }
    return error("Unexpected character.");
}
