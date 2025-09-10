#include "monitor/watchpoint.h"
#include "monitor/expr.h"

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


