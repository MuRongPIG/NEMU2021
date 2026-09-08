#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
	int NO;
	struct watchpoint *next;

	/* The expression to be watched, and its last evaluated value. */
	char expr_str[32];
	uint32_t value;
} WP;

void init_wp_pool();
WP* new_wp();
void free_wp(WP *wp);
void set_watchpoint(char *expr_str);
void delete_watchpoint(int NO);
void print_watchpoints();
bool check_watchpoints();

#endif
