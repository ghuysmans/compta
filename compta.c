#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <getopt.h>
#include <err.h>
#include <errno.h>
#include <error.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "compta.h"

void redirect(const char *driver);

struct account *accounts = NULL;
int verbose = 0;
FILE *output;
int op_num=0;

char *s=NULL;
size_t l=0;

#define USAGE \
	"usage: %s [-P pcmn] [-T driver] [-v] f|- [...]\n" \
	"see manual.ps for details.\n"

#define DELIM " \t;"

/**
 * Gets a node for the given account number.
 * If it doesn't exist, creates and registers a new one.
 */
struct account *getaccount(int number) {
	struct account *node = accounts;
	if (number < 1)
		errx(-3, "invalid account number %d", number);
	while (node && node->number!=number)
		node = node->next;
	if (!node) {
		node = malloc(sizeof(struct account));
		if (!node)
			errx(-1, "malloc");
		node->number = number;
		node->description = NULL;
		node->operations = NULL;
		node->next = accounts;
		accounts = node;
	}
	return node;
}

void loadpcmn(char *filename) {
	FILE *f = fopen(filename, "r");
	int line = 1;
	if (!f)
		err(errno, filename);
	while (getline(&s, &l, f) != -1) {
		size_t num_len=strcspn(s, DELIM), line_len=strlen(s);
		int num = atoi(s);
		if (!num || num_len+2==line_len || isdigit(*(s+num_len+1)))
			error_at_line(-2, 0, filename, line, "expected text");
		else {
			struct account *node = getaccount(num);
			s[line_len-1] = 0; //terminate the description
			//TODO check for tabs
			if (!(node->description = strdup(s+num_len+1)))
				errx(-1, "strdup");
		}
		line++;
	}
	fclose(f);
}

void checkbalance(const char *filename, int line, int balance) {
	if (balance)
		error_at_line(-4, 0, filename, line, "uneven balance: %d", balance);
}

void parsetransactions(char *filename) {
	FILE *f;
	int line=1;
	int acc=0, first=1;
	if (*filename == '-')
		f = stdin;
	else if (!(f = fopen(filename, "r")))
		err(errno, filename);
	while (getline(&s, &l, f) != -1) {
		if (*s == '\n') {
			checkbalance(filename, line, acc);
			acc = 0;
			if (!first)
				hook.operation_footer(filename);
			first = 1;
			op_num++;
		}
		else if (*s == ';')
			hook.raw(filename, s+1);
		else {
			char *p;
			if (!(p = strtok(s, DELIM)))
				error_at_line(-2, 0, filename, line, "syntax error");
			else {
				struct account *account;
				int sign = 1;
				if (*s == '>') {
					sign = -1;
					account = getaccount(atoi(s+1));
				}
				else
					account = getaccount(atoi(s));
				if (!(p = strtok(NULL, DELIM "\n")))
					error_at_line(-2, 0, filename, line, "syntax error");
				else {
					struct account_op *op;
					if (!(op = malloc(sizeof(struct account_op))))
						errx(-1, "malloc");
					if (!isdigit(*p))
						error_at_line(-2, 0, filename, line, "expected number");
					acc += op->amount = sign*atoi(p);
					op->number = op_num;
					op->next = account->operations;
					if (p = strtok(NULL, "\n")) {
						if (account->description)
							free(account->description);
						//TODO check for tab
						if (isdigit(*p))
							error_at_line(-2, 0, filename, line, "expected text");
						else if (!(account->description = strdup(p)))
							errx(-1, "strdup");
					}
					account->operations = op;
					if (first) {
						hook.operation_header(filename, line);
						first = 0;
					}
					hook.operation(filename, account, op, line);
				}
			}
		}
		line++;
	}
	if (!first)
		hook.operation_footer(filename);
	checkbalance(filename, line, acc);
	fclose(f);
}

void freeaccounts() {
	struct account *v = accounts;
	while (v) {
		struct account *nx = v->next;
		struct account_op *op = v->operations;
		while (op) {
			struct account_op *nx2 = op->next;
			free(op);
			op = nx2;
		}
		if (v->description)
			free(v->description);
		free(v);
		v = nx;
	}
}

int main(int argc, char *argv[]) {
	char *driver=NULL, *load=NULL;
	int o;
	output = stdout;
	while ((o=getopt(argc, argv, "T:P:l:v")) != -1) {
		switch (o) {
		case 'T':
			driver = optarg;
			break;
		case 'P':
			loadpcmn(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'l':
			load = optarg;
			break;
		case '?':
			return 1;
		}
	}
	if (optind == argc) {
		fprintf(stderr, USAGE, argv[0]);
		return 1;
	}
	if (load) {
		void *dl;
		void (*setup)(const char *driver);
		if (!(dl = dlopen(load, RTLD_NOW)))
			errx(-5, "dlopen: %s", dlerror());
		else if (!(setup = dlsym(dl, "setuphook")))
			errx(-6, "dlsym: %s", dlerror());
		else {
			setup(driver);
			if (verbose)
				fprintf(stderr, "hook installed.\n");
		}
	}
	else
		redirect(driver);
	hook.header();
	for (; optind < argc; optind++)
		parsetransactions(argv[optind]);
	hook.printaccounts();
	freeaccounts();
	free(s);
	return 0;
}
