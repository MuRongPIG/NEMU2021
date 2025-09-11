#include "monitor/watchpoint.h"
#include "monitor/expr.h"
#include "cpu/reg.h"
#include <stdlib.h>

#define NR_WP 32

// 监视点“池”
static WP wp_pool[NR_WP];
// 链表 head: 组织使用中的监视点
// 链表 free_: 组织空闲的监视点
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

/* TODO: Implement the functionality of watchpoint */

// 创建新的监视点
static WP* new_wp() {
	assert(free_ != NULL);
	WP *wp = free_;
	free_ = free_->next;
	return wp;
}

// 释放监视点
// static void free_wp(WP *wp) {
// 	assert(wp >= wp_pool && wp < wp_pool + NR_WP);
// 	free(wp->expr);
// 	wp->next = free_;
// 	free_ = wp;
// }

// 设置新的监视点，并返回其编号
int set_watchpoint(char *args) {
	uint32_t val;
	bool success;
	val = expr(args,&success);
	if(!success) {
		return -1;
	}

	WP *wp = new_wp();
	wp->expr = strdup(args);
	wp->old_val = val;

	wp->next = head;
	head = wp;
	return wp->NO;
}

// 扫描全部监视点，返回发生变化的监视点个数
int scan_watchpoint() {
	int n = 0;
	WP *wp = head;
	while(wp != NULL) {
		bool success;
		int val = expr(wp->expr,&success);
		assert(success);

		if(val != wp->old_val) {
			n++;
			printf("Hint watchpoint %d at address 0x%08x\n",wp->NO,get_reg_val("eip",&success));
		}

		wp->old_val = val;
		wp = wp->next;
	}
	return n;
}