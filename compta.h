#ifndef _COMPTA_H
#define _COMPTA_H

#include <stdio.h>

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

struct hook {
	//TODO add a version field? how should we handle it?
	void (*header)();
	void (*raw)(const char *filename, char *s);
	void (*operation_header)(const char *filename, int line);
	void (*operation)(const char *filename,
		const struct account *account, const struct account_op *op, int line);
	void (*operation_footer)(const char *filename);
	void (*printaccounts)();
};

extern struct account *accounts;
extern int verbose;
extern FILE *output;
extern int op_num;

extern char *s;
extern size_t l;

extern struct hook hook;

void setuphook(const char *driver);

#endif //_COMPTA_H
