#include <stdio.h>
#include <getopt.h>
#include <err.h>
#include <errno.h>
#include <error.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

struct account {
	int number;
	char *description;
	struct account_op *operations;
	struct account *next;
};

struct account_op {
	int amount; //positive means debit
	int number; //FIXME
	struct account_op *next;
};

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

#define HEADER ".2C\n.ps 12\nOpérations\n\n.ev table\n.ps 10\n"
#define OPS_HEADER \
	".KS\n.TS\ndecimalpoint(,);\ncb s s s\nn | n || c || l.\n" \
	"%d @ %s:%d\n_\n"
#define OPS_FOOTER ".TE\n.KE\n"
#define MIDDLE "\n.bp\n.ev\nComptes\n\n.ev table\n"
#define ACC_HEADER \
	".KS\n.TS\ndecimalpoint(,);\ncb s s\nni | n | n.\n%d %s\n_\n"
#define ACC_FOOTER ".TE\n.KE\n"

#define FILTER "groff -Dutf8 -t -T "

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
			if (acc)
				error_at_line(-4, 0, filename, line, "uneven balance");
			acc = 0;
			if (!first)
				fprintf(output, OPS_FOOTER);
			first = 1;
			op_num++;
		}
		else if (*s == ';')
			fprintf(output, "%s", s+1);
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
					char *f = sign<0 ? "\t%d\t%s\t%d\n" : "%d\t\t%s\t%d\n";
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
						fprintf(output, OPS_HEADER, op_num, filename, line);
						first = 0;
					}
					fprintf(output, f, abs(op->amount), account->description, account->number);
				}
			}
		}
		line++;
	}
	if (!first)
		fprintf(output, OPS_FOOTER);
	if (acc)
		error_at_line(-4, 0, filename, line, "uneven balance");
	fclose(f);
}

void output_account_operations(struct account_op *operation) {
	if (operation) {
		char *f = operation->amount<0 ? "%d\t\t%d\n" : "%d\t%d\n";
		output_account_operations(operation->next);
		fprintf(output, f, operation->number, abs(operation->amount));
	}
}

void output_accounts(struct account *account) {
	if (account) {
		output_accounts(account->next);
		if (verbose || account->operations) {
			fprintf(output, ACC_HEADER, account->number, account->description);
			output_account_operations(account->operations);
			fprintf(output, ACC_FOOTER);
		}
	}
}

void flushtogroff() {
	pclose(output);
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
	char *driver=NULL;
	int o;
	while ((o=getopt(argc, argv, "T:P:v")) != -1) {
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
		case '?':
			return 1;
		}
	}
	if (optind == argc) {
		fprintf(stderr, USAGE, argv[0]);
		return 1;
	}
	if (driver) {
		//FIXME filter this?
		char *buf = malloc(strlen(FILTER)+strlen(driver)+1);
		if (!buf)
			errx(-1, "malloc");
		strcpy(buf, FILTER);
		strcat(buf, driver);
		output = popen(buf, "w");
		free(buf);
		if (!output)
			err(errno, "popen");
		atexit(flushtogroff);
	}
	else
		output = stdout;
	fprintf(output, HEADER);
	for (; optind < argc; optind++)
		parsetransactions(argv[optind]);
	fprintf(output, MIDDLE);
	output_accounts(accounts);
	freeaccounts();
	free(s);
	return 0;
}
