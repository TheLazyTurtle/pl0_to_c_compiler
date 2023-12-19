#include <cstdarg>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstddef>
#include <stdlib.h>
#include <limits.h>
#include <stdlib.h>
#include <cctype>
#include <stdio.h>
#include <malloc.h>
#include <ctype.h>
#include "tokens.h"

static char *raw;
static size_t line = 1;

static char* token;
static int type;

static void error (const char* fmt, ...) {
	va_list ap;
	(void) fprintf(stderr, "pl0c: error: %lu: ", line);

	va_start(ap, fmt);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);

	(void) fputc('\n', stderr);

	exit(1);
}

static void comment(void) {
	int ch;
	while ((ch = *raw++) != '}') {
		if (ch == '\0') {
			error("comment(): unterminated commend");
		}

		if (ch == '\n') {
			line++;
		}
	}
}

static int ident(void) {
	char* p;
	size_t i, len;

	p = raw;
	while (isalnum(*raw) || *raw == '_') {
		raw++;
	}

	len = raw - p;
	raw--;

	free(token);

	if ((token = (char*) malloc(len + 1)) == NULL) {
		error("Malloc failed at ident");
	}

	for (i = 0; i < len; i++) {
		token[i] = *p++;
	}

	token[i] = '\0';

	if (!strcmp(token, "const"))
		return TOK_CONST;
	else if (!strcmp(token, "var"))
		return TOK_VAR;
	else if (!strcmp(token, "procedure"))
		return TOK_PROCEDURE;
	else if (!strcmp(token, "call"))
		return TOK_CALL;
	else if (!strcmp(token, "begin"))
		return TOK_BEGIN;
	else if (!strcmp(token, "end"))
		return TOK_END;
	else if (!strcmp(token, "if"))
		return TOK_IF;
	else if (!strcmp(token, "then"))
		return TOK_THEN;
	else if (!strcmp(token, "while"))
		return TOK_WHILE;
	else if (!strcmp(token, "do"))
		return TOK_DO;
	else if (!strcmp(token, "odd"))
		return TOK_ODD;

	return TOK_IDENT;
}

static int number(void) {
	char* p;
	size_t i, j = 0, len;

	p = raw;
	while(isdigit(*raw) || *raw == '_') {
		raw++;
	}

	len = raw - p;
	raw--;

	free(token);

	if ((token = (char*) malloc(len + 1)) == NULL) {
		error("Malloc failed at number");
	}

	for (i = 0; i < len; i++) {
		if (isdigit(*p)) {
			token[j++] = *p;
		}
		p++;
	}

	token[j] = '\0';

	char* endptr;
	unsigned long num = strtoul(token, &endptr, 10);

	if (*endptr != '\0' || endptr == token) {
		error("invalid number: %s\n", token);
	}
	else if (num > LONG_MAX) {
		error("number exceeds LONG_MAX: %lu\n", num);
	}

	return TOK_NUMBER;
}

static int lex(void) {
again: 
	while (*raw == ' ' || *raw == '\t' || *raw == '\n') {
		if (*raw++ == '\n') {
			line++;
		}
	}

	if (isalpha(*raw) || *raw == '_') {
		return ident();
	}

	if (isdigit(*raw)) {
		return number();
	}

	switch (*raw) {
		case '{':
			comment();
			goto again;
		case '.':
		case '=':
		case ',':
		case ';':
		case '#':
		case '<':
		case '>':
		case '+':
		case '-':
		case '*':
		case '/':
		case '(':
		case ')':
			return (*raw);
		case ':':
			if (*raw++ != '=')
				error("unknown token: ':%c'", *raw);
		case '\0':
			return 0;
		default:
			error("unknown token: '%c'", *raw);
	}

	return 0;
}

void parse(void) {
	while ((type = lex()) != 0) 
	{
		raw++;
		printf("%lu|%d\t", line, type);
		switch (type) {
			case TOK_IDENT:
			case TOK_NUMBER:
			case TOK_CONST:
			case TOK_VAR:
			case TOK_PROCEDURE:
			case TOK_CALL:
			case TOK_BEGIN:
			case TOK_END:
			case TOK_IF:
			case TOK_THEN:
			case TOK_WHILE:
			case TOK_DO:
			case TOK_ODD:
				printf("%s", token);
				break;
			case TOK_DOT:
			case TOK_EQUAL:
			case TOK_COMMA:
			case TOK_SEMICOLON:
			case TOK_HASH:
			case TOK_LESSTHAN:
			case TOK_GREATERTHAN:
			case TOK_PLUS:
			case TOK_MINUS:
			case TOK_MULTIPLY:
			case TOK_DIVIDE:
			case TOK_LPAREN:
			case TOK_RPAREN:
				printf("%d", type);
				break;
			case TOK_ASSIGN:
				printf(":=");
		}
		printf("\n");
	}
}

static void readin(char* file) {
	int fd;
	struct stat st;

	if (strrchr(file, '.') == NULL)
		error("file must end in '.pl0'");

	if (!!strcmp(strrchr(file, '.'), ".pl0"))
		error("file must end in '.pl0'");

	if ((fd = open(file, O_RDONLY)) == -1)
		error("couldn't open %s", file);

	if (fstat(fd, &st) == -1)
		error("couldn't get file size");

	if ((raw = (char*)malloc(st.st_size + 1)) == NULL)
		error("malloc failed");

	if (read(fd, raw, st.st_size) != st.st_size)
		error("couldn't read %s", file);
	raw[st.st_size] = '\0';

	(void) close(fd);
}

int main (int argc, char* argv[]) {
	char* startp;

	if (argc != 2) {
		printf("usage: pl0c file.pl0");
		exit(1);
	}

	readin(argv[1]);
	startp = raw;

	parse();
	free(startp);

	return 0;
}

