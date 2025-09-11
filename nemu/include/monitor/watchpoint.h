#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
	int NO;
	struct watchpoint *next;

	/* TODO: Add more members if necessary */
	char *expr;
	uint32_t old_val;
} WP;

#endif

int set_watchpoint(char *e);
int scan_watchpoint();
bool delete_watchpoint(int NO);