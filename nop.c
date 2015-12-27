#define _COMPTA_HOOK
#include <stdio.h>
#include "compta.h"

void header() {
}

void raw(const char *filename, char *s) {
	fprintf(output, "%s: %s", filename, s);
}

void operation_header(const char *filename, int line) {
}

void operation(const char *filename, const struct account *account, const struct account_op *op, int line) {
}

void operation_footer(const char *filename) {
}

void printaccounts(const char *filename) {
}

void setuphook(const char *driver) {
	hook.header = header;
	hook.raw = raw;
	hook.operation_header = operation_header;
	hook.operation = operation;
	hook.operation_footer = operation_footer;
	hook.printaccounts = printaccounts;
}
