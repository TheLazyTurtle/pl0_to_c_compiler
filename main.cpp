#include <cstdarg>
#include <endian.h>
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
#include "strtonum.c"

#define PL0C_VERSION "1.0.0"
#define CHECK_LHS 0 
#define CHECK_RHS 1
#define CHECK_CALL 2

typedef struct symtab {
	int depth;
	int type;
	char* name;
	struct symtab* next;
} symbolTabel;
static symbolTabel* head;

static char *raw;
static size_t line = 1;
static size_t depth = 0;

static char* token;
static int type;
static int proc;

static void error (const char* fmt, ...) {
	va_list ap;
	(void) fprintf(stderr, "pl0c: error: %lu: ", line);

	va_start(ap, fmt);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);

	(void) fputc('\n', stderr);

	exit(1);
}

/*
	Semantics
*/

static void symcheck(int check) {
	symbolTabel *curr, *ret = NULL;

	curr = head;
	while (curr != NULL) {
		if (!strcmp(token, curr->name)) {
			ret = curr;
		}

		curr = curr->next;
	}

	if (ret == NULL) {
		error("undefined symbol: %s", token);
	}

	switch(check) {
		case CHECK_LHS:
			if (ret->type != TOK_VAR) {
				error("must be a variable: %s", token);
			}
			break;
		case CHECK_RHS:
			if (ret->type == TOK_PROCEDURE) {
				error("must not be a procedure: %s", token);
			}
			break;
		case CHECK_CALL:
			if (ret->type != TOK_PROCEDURE) {
				error("must be a procedure: %s", token);
			}
	}
}

/*
	Lexing
*/

static void comment(void) {
	int ch;
	while ((ch = *raw++) != '}') {
		if (ch == '\0') {
			error("comment(): unterminated commend");
		}

		if (ch == '\n') {
			++line;
		}
	}
}

static int ident(void) {
	char* p;
	size_t i, len;

	p = raw;
	while (isalnum(*raw) || *raw == '_') {
		++raw;
	}

	len = raw - p;
	--raw;

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
	const char *errstr;
	char* p;
	size_t i, j = 0, len;

	p = raw;
	while(isdigit(*raw) || *raw == '_') {
		++raw;
	}

	len = raw - p;
	--raw;

	free(token);

	if ((token = (char*) malloc(len + 1)) == NULL) {
		error("Malloc failed at number");
	}

	for (i = 0; i < len; i++) {
		if (isdigit(*p)) {
			token[j++] = *p;
		}
		++p;
	}

	token[j] = '\0';

	(void) strtonum(token, 0, LONG_MAX, &errstr);
	if (errstr != NULL)
		error("invalid number: %s", token);

	return TOK_NUMBER;
}

static int lex(void) {
again: 
	while (*raw == ' ' || *raw == '\t' || *raw == '\n') {
		if (*raw++ == '\n') {
			++line;
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
			if (*++raw != '=') {
				error("unknown token: ':%c'", *raw);
			}
			return TOK_ASSIGN;
		case '\0':
			return 0;
		default:
			error("unknown token: '%c'", *raw);
	}

	return 0;
}

/*
	Code Gen
*/

static void aout(const char* fmt, ...) {
	va_list ap;
	va_start (ap, fmt);
	(void) vfprintf(stdout, fmt, ap);

	va_end(ap);
}

static void cg_end(void) {
	aout("/* PL/0 compiler %s */\n", PL0C_VERSION);
}

static void cg_const(void) {
	aout("const long %s=", token);
}

static void cg_semicolon(void) {
	aout(";\n");
}

static void cg_crlf(void) {
	aout("\n");
}

static void cg_var(void) {
	aout("long %s;\n", token);
}

static void cg_procedure(void) {
	if (proc == 0) {
		aout("int\n");
		aout("main(int argc, char* argv[])\n");
	}
	else {
		aout("void\n");
		aout("%s(void)\n", token);
	}

	aout("{\n");
}


static void cg_epilogue(void) {
	aout(";");

	if (proc == 0) {
		aout("return 0;");
	}

	aout("\n}\n\n");
}

static void cg_call(void) {
	aout("%s();\n", token);
}

static void cg_odd(void) {
	aout(")&1");
}

static void cg_symbol(void) {
	switch(type) {
		case TOK_IDENT:
		case TOK_NUMBER:
			aout("%s", token);
			break;
		case TOK_BEGIN:
			aout("{\n");
			break;
		case TOK_END:
			aout(";\n}\n");
			break;
		case TOK_IF:
			aout("if(");
			break;
		case TOK_THEN:
		case TOK_DO:
			aout(")");
			break;
		case TOK_ODD:
			aout("(");
			break;
		case TOK_WHILE:
			aout("while(");
			break;
		case TOK_EQUAL:
			aout("==");
			break;
		case TOK_COMMA:
			aout(",");
			break;
		case TOK_ASSIGN:
			aout("=");
			break;
		case TOK_HASH:
			aout("!=");
			break;
		case TOK_LESSTHAN:
			aout("<");
			break;
		case TOK_GREATERTHAN:
			aout(">");
			break;
		case TOK_PLUS:
			aout("+");
			break;
		case TOK_MINUS:
			aout("-");
			break;
		case TOK_MULTIPLY:
			aout("*");
			break;
		case TOK_DIVIDE:
			aout("/");
			break;
		case TOK_LPAREN:
			aout("(");
			break;
		case TOK_RPAREN:
			aout(")");
	}
}

static void addsymbol(int type) {
	symbolTabel *curr, *newNode;
	curr = head;
	while (1) {
		if (!strcmp(curr->name, token)) {
			if (curr->depth == (depth - 1)) {
				error("duplicate symbol: %s", token);
			}
		}

		if (curr->next == NULL) {
			break;
		}

		curr = curr->next;
	}

	if ((newNode = (symbolTabel*)malloc(sizeof(symbolTabel))) == NULL) {
		error("Malloc failed at addsymbol");
	}

	newNode->depth = depth - 1;
	newNode->type = type;

	if ((newNode->name = strdup(token)) == NULL) {
		error("Malloc failed at addsymbol while duplicating name");
	}

	newNode->next = NULL;
	curr->next = newNode;
}

static void destroysymbols(void) {
	symbolTabel *curr, *prev;

again:
	curr = head;
	while (curr->next != NULL) {
		prev = curr;
		curr = curr->next;
	}

	if (curr->type != TOK_PROCEDURE) {
		free(curr->name);
		free(curr);
		prev->next = NULL;
		goto again;
	}
}



/*
	Parsing
*/

static void next(void) {
	type = lex();
	++raw;
}

static void expect(int match) {
	if (match != type) {
		error("syntax error");
	}

	next();
}

static void expression(void);

static void factor(void) {
	switch (type) {
		case TOK_IDENT:
			symcheck(CHECK_RHS);
			// intential no break
		case TOK_NUMBER:
			cg_symbol();
			next();
			break;
		case TOK_LPAREN:
			cg_symbol();
			expect(TOK_LPAREN);
			expression();
			if (type == TOK_RPAREN)
				cg_symbol();
			expect(TOK_RPAREN);
	}
}

static void term(void) {
	factor();
	while (type == TOK_MULTIPLY || type == TOK_DIVIDE) {
		cg_symbol();
		next();
		factor();
	}
}

static void expression(void) {
	if (type == TOK_PLUS || type == TOK_MINUS) {
		cg_symbol();
		next();
	}

	term();
	while (type == TOK_PLUS || type == TOK_MINUS) {
		cg_symbol();
		next();
		term();
	}
}

static void condition(void) {
	if (type == TOK_ODD) {
		cg_symbol();
		expect(TOK_ODD);
		expression();
		cg_odd();
	}
	else {
		expression();

		switch (type) {
			case TOK_EQUAL:
			case TOK_HASH:
			case TOK_LESSTHAN:
			case TOK_GREATERTHAN:
				cg_symbol();
				next();
				break;
			default:
				error("invalid conditional");
		}

		expression();
	}
}

static void statement(void) {
	switch (type) {
		case TOK_IDENT:
			symcheck(CHECK_LHS);
			cg_symbol();

			expect(TOK_IDENT);
			if (type == TOK_ASSIGN) {
				cg_symbol();
			}

			expect(TOK_ASSIGN);
			expression();
			break;
		case TOK_CALL:
			expect(TOK_CALL);
			if (type == TOK_IDENT) {
				symcheck(CHECK_CALL);
				cg_call();
			}
			expect(TOK_IDENT);
			break;
		case TOK_BEGIN:
			cg_symbol();
			expect(TOK_BEGIN);
			statement();
			while (type == TOK_SEMICOLON) {
				cg_semicolon();
				expect(TOK_SEMICOLON);
				statement();
			}
			if (type == TOK_END) {
				cg_symbol();
			}
			expect(TOK_END);
			break;
		case TOK_IF:
			cg_symbol();
			expect(TOK_IF);
			condition();
			if (type == TOK_THEN) {
				cg_symbol();
			}
			expect(TOK_THEN);
			statement();
			break;
		case TOK_WHILE:
			cg_symbol();
			expect(TOK_WHILE);
			condition();
			if (type == TOK_DO) {
				cg_symbol();
			}
			expect(TOK_DO);
			statement();
		// No default by design
	}
}

static void block(void) {
	if (depth++ > 1) {
		error("nesting depth exceeded");
	}

	if (type == TOK_CONST) {
		expect(TOK_CONST);
		if (type == TOK_IDENT) {
			addsymbol(TOK_CONST);
			cg_const();
		}
		expect(TOK_IDENT);
		expect(TOK_EQUAL);
		if (type == TOK_NUMBER) {
			cg_symbol();
			cg_semicolon();
		}
		expect(TOK_NUMBER);

		while (type == TOK_COMMA) {
			expect(TOK_COMMA);
			if (type == TOK_IDENT) {
				addsymbol(TOK_CONST);
				cg_const();
			}
			expect(TOK_IDENT);
			expect(TOK_EQUAL);
			if (type == TOK_NUMBER) {
				cg_symbol();
				cg_semicolon();
			}
			expect(TOK_NUMBER);
		}
		expect(TOK_SEMICOLON);
	}

	if (type == TOK_VAR) {
		expect(TOK_VAR);
		if (type == TOK_IDENT) { 
			addsymbol(TOK_VAR);
			cg_var();
		}
		expect(TOK_IDENT);

		while (type == TOK_COMMA) {
			expect(TOK_COMMA);
			if (type == TOK_IDENT) {
				addsymbol(TOK_VAR);
				cg_var();
			}
			expect(TOK_IDENT);
		}

		expect(TOK_SEMICOLON);
		cg_crlf();
	}

	while (type == TOK_PROCEDURE) {
		proc = 1;

		expect(TOK_PROCEDURE);
		if (type == TOK_IDENT) {
			addsymbol(TOK_PROCEDURE);
			cg_procedure();
		}
		expect(TOK_IDENT);
		expect(TOK_SEMICOLON);

		block();
		expect(TOK_SEMICOLON);

		proc = 0;
		destroysymbols();
	}

	if (proc == 0) {
		cg_procedure();
	}

	statement();

	cg_epilogue();

	if (--depth < 0) {
		error("nesting depth fell below 0");
	}
}

static void parse(void) {
	next();
	block();
	expect(TOK_DOT);

	if (type != 0)
		error("extra tokens at end of file");

	cg_end();
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

static void initsymtab(void) {
	symbolTabel* newNode;
	if ((newNode = (symbolTabel*)malloc(sizeof(struct symtab))) == NULL) {
		error("Malloc failed at initsymtab");
	}

	newNode->depth = 0;
	newNode->type = TOK_PROCEDURE;
	newNode->name = "main";
	newNode->next = NULL;

	head = newNode;
}

int main (int argc, char* argv[]) {
	char* startp;

	if (argc != 2) {
		printf("usage: pl0c file.pl0");
		exit(1);
	}

	readin(argv[1]);
	startp = raw;

	initsymtab();
	parse();
	free(startp);

	return 0;
}

