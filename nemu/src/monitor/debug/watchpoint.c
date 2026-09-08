#include "monitor/watchpoint.h"
#include "monitor/expr.h"
#include "nemu.h"

#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;

void init_wp_pool() {
	int i;
	for(i = 0; i < NR_WP; i ++) {
		wp_pool[i].NO = i;
		wp_pool[i].next = &wp_pool[i + 1];
	}
	wp_pool[NR_WP - 1].next = NULL;

	head = NULL;
	free_ = wp_pool;
}

/* Return a free watchpoint from the free list. */
WP* new_wp() {
	Assert(free_ != NULL, "no free watchpoint in the pool");
	WP *wp = free_;
	free_ = free_->next;
	wp->next = NULL;
	return wp;
}

/* Return a watchpoint back to the free list. */
void free_wp(WP *wp) {
	wp->next = free_;
	free_ = wp;
}

/* Set a new watchpoint with the given expression. */
void set_watchpoint(char *expr_str) {
	WP *wp = new_wp();

	strncpy(wp->expr_str, expr_str, 31);
	wp->expr_str[31] = '\0';

	bool success = true;
	wp->value = expr(wp->expr_str, &success);
	if(!success) {
		printf("Invalid expression: %s\n", expr_str);
		free_wp(wp);
		return;
	}

	wp->next = head;
	head = wp;
	printf("Watchpoint %d: %s\n", wp->NO, wp->expr_str);
}

/* Delete the watchpoint with the given NO. */
void delete_watchpoint(int NO) {
	WP *prev = NULL, *wp = head;
	while(wp != NULL && wp->NO != NO) {
		prev = wp;
		wp = wp->next;
	}

	if(wp == NULL) {
		printf("No watchpoint %d\n", NO);
		return;
	}

	if(prev == NULL) { head = wp->next; }
	else { prev->next = wp->next; }

	free_wp(wp);
	printf("Watchpoint %d deleted\n", NO);
}

/* Print all watchpoints in use. */
void print_watchpoints() {
	WP *wp;
	if(head == NULL) {
		printf("No watchpoints.\n");
		return;
	}
	for(wp = head; wp != NULL; wp = wp->next) {
		printf("Watchpoint %d: %s = 0x%08x\n", wp->NO, wp->expr_str, wp->value);
	}
}

/* Check all watchpoints after executing an instruction.
 * If the value of any watchpoint has changed, print a hint and return true.
 */
bool check_watchpoints() {
	WP *wp;
	for(wp = head; wp != NULL; wp = wp->next) {
		bool success = true;
		uint32_t new_val = expr(wp->expr_str, &success);
		if(!success) { continue; }

		if(new_val != wp->value) {
			wp->value = new_val;
			printf("Hint watchpoint %d at address 0x%08x\n", wp->NO, cpu.eip);
			return true;
		}
	}
	return false;
}


