#include "compta.h"
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>

#define HEADER ".2C\n.ps 12\nOpérations\n\n.ev table\n.ps 10\n"
#define OPS_HEADER \
	".KS\n.TS\ndecimalpoint(,);\ncb s s s\nn | n || c || l.\n" \
	"%d @ %s:%d\n_\n"
#define OPS_FOOTER ".TE\n.KE\n\n"
#define MIDDLE "\n.bp\n.ev\nComptes\n\n.ev table\n"
#define ACC_HEADER \
	".KS\n.TS\ndecimalpoint(,);\ncb s s\nni | n | n.\n%d %s\n_\n"
#define ACC_FOOTER ".TE\n.KE\n\n"

#define FILTER "groff -Dutf8 -t -T "

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

void operation_footer(const char *filename) {
	fprintf(output, OPS_FOOTER);
}

void header() {
	fprintf(output, HEADER);
}

void printaccounts() {
	fprintf(output, MIDDLE);
	output_accounts(accounts);
}

void raw(const char *filename, char *s) {
	fprintf(output, "%s", s);
}

void operation_header(const char *filename, int line) {
	fprintf(output, OPS_HEADER, op_num, filename, line);
}

void operation(const char *filename, const struct account *account, const struct account_op *op, int line) {
	char *f = op->amount<0 ? "\t%d\t%s\t%d\n" : "%d\t\t%s\t%d\n";
	fprintf(output, f, abs(op->amount), account->description, account->number);
}

void flushtogroff() {
	pclose(output);
}

void redirect(const char *driver) {
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
}

//default pointers
struct hook hook = {header, raw,
	operation_header, operation, operation_footer, printaccounts};
