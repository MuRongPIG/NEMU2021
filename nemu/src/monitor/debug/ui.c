#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint32_t);

/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
	static char *line_read = NULL;

	if (line_read) {
		free(line_read);
		line_read = NULL;
	}

	line_read = readline("(nemu) ");

	if (line_read && *line_read) {
		add_history(line_read);
	}

	return line_read;
}

static int cmd_c(char *args) {
	cpu_exec(-1);
	return 0;
}

static int cmd_q(char *args) {
	return -1;
}

static int cmd_help(char *args);

static int cmd_si(char *args);

static int cmd_info(char *args);

static int cmd_x(char *args);

static int cmd_p(char *args);

static struct {
	char *name;
	char *description;
	int (*handler) (char *);
} cmd_table [] = {
	{ "help", "Display informations about all supported commands", cmd_help },
	{ "c", "Continue the execution of the program", cmd_c },
	{ "q", "Exit NEMU", cmd_q },
	{ 
		"si", 
		"\tStep one instruction exactly.\n"
		"\tUsage: si [N]\n"
		"\tArgument N means step N times (n till program stops for another reason).",
		cmd_si
	},
	{ "info", "Generic command for showing things about the program being debugged.", cmd_info },
	{ "x", "Scan the memory." , cmd_x},
	{ "p", "Calculate an expression." , cmd_p},
	/* TODO: Add more commands */
};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
	/* extract the first argument */
	char *arg = strtok(NULL, " ");
	int i;

	if(arg == NULL) {
		/* no argument given */
		for(i = 0; i < NR_CMD; i ++) {
			printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
		}
	}
	else {
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(arg, cmd_table[i].name) == 0) {
				printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
				return 0;
			}
		}
		printf("Unknown command '%s'\n", arg);
	}
	return 0;
}

static int cmd_si(char *args) {
	/* 已经在 ui_mainloop 中第一次使用 strtok,现在使用 NULL 表示继续分割同一个字符串 */
	char *arg = strtok(NULL, " ");
	int cnt = 0;
	// 没有传参，默认值为 1
	if(arg == NULL) {
		cnt = 1;
	}
	else {
		cnt = atoi(arg);
	}
	// assert(cnt > 0);
	if(cnt <= 0) {
		printf("The argument should be a positive integer.\n");
	}
	else {
		cpu_exec(cnt);
	}
	return 0;
}

// 此处传入的 args 应当就是除了指令之外的全部参数，不需要再分割
static int cmd_info(char *args) {
	if(args == NULL) {
		printf("Require more arguments.\n");
	}
	else if(strcmp(args, "r") == 0) {
		int i;
		for(i = R_EAX; i <= R_EDI; i ++) {;
			printf("%s\t0x%08x\t%d\n",regsl[i],reg_l(i),reg_l(i));
		}
		printf("eip\t0x%08x\t%d\n",cpu.eip,cpu.eip);
	}
	else {
		printf("Invalid command.\n");
	}
	return 0;
}

// 目前只实现读取十六进制整数作为表达式的值，之后需要将其修改为读取表达式并求值
static int cmd_x(char *args) {
	char *arg = strtok(NULL, " ");
	Log("%s\n",arg);
	if(arg == NULL) {
		printf("Require more arguments.\n");
		return 0;
	}
	int n;
	swaddr_t addr;
	sscanf(arg, "%d", &n);
	bool success;
	addr = expr(arg + strlen(arg) + 1, &success);
	if(!success) {
		printf("Bad expression.\n");
		return 0;
	}
	Log("cmd_x args: %d %d\n",n,addr);
	int i;
	// 四个四个输出
	for(i = 0; i < n/4; ++i) {
		printf("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x\n",addr,swaddr_read(addr,4),swaddr_read(addr+4,4),swaddr_read(addr+8,4),swaddr_read(addr+12,4));
		addr += 16;
	}
	if(n % 4 != 0) {
		printf("0x%08x:",addr);
		for(i = 0; i < n % 4; ++i) {
			printf(" 0x%08x",swaddr_read(addr,4));
			addr += 4;
		}
		printf("\n");
	}
	return 0;
}

// static int cmd_x(char *args) {
//     if(args == NULL) {
//         printf("Require more arguments.\n");
//         return 0;
//     }
//     int n;
//     // 读取第一个整数和它在字符串中的位置信息
//     int chars_read;
//     int count = sscanf(args, "%d%n", &n, &chars_read);
//     if(count != 1) {
//         printf("Require integer as first argument.\n");
//         return 0;
//     }
//     // 表达式部分从读取的字符数之后开始
//     char *expr_start = args + chars_read;
//     // 跳过可能的空格
//     while(*expr_start == ' ') {
//         expr_start++;
//     }
//     if(*expr_start == '\0') {
//         printf("Require address expression.\n");
//         return 0;
//     }
//     bool success;
//     swaddr_t addr = expr(expr_start, &success);
//     if(!success) {
//         printf("Bad expression: %s\n", expr_start);
//         return 0;
//     }

//     Log("cmd_x args: %d %d\n",n,addr);
	
// 	int i;
// 	// 四个四个输出
// 	for(i = 0; i < n/4; ++i) {
// 		printf("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x\n",addr,swaddr_read(addr,4),swaddr_read(addr+4,4),swaddr_read(addr+8,4),swaddr_read(addr+12,4));
// 		addr += 16;
// 	}
// 	if(n % 4 != 0) {
// 		printf("0x%08x:",addr);
// 		for(i = 0; i < n % 4; ++i) {
// 			printf(" 0x%08x",swaddr_read(addr,4));
// 			addr += 4;
// 		}
// 		printf("\n");
// 	}
// 	return 0;
// }

static int cmd_p(char *args) {
	bool success;
	if(args == NULL) {
		printf("Require more arguments.\n");
	}
	else {
		uint32_t res = expr(args, &success);
		if(success) {
			printf("0x%08x(%d)\n",res,res);
		}
		else {
			printf("Bad expression.\n");
		}
	}
	return 0;
}

void ui_mainloop() {
	while(1) {
		char *str = rl_gets();
		char *str_end = str + strlen(str);

		/* extract the first token as the command */
		/* 第一次使用 strtok 需要传入字符串，之后分割同一个字符串时传入 NULL */
		char *cmd = strtok(str, " ");
		if(cmd == NULL) { continue; }

		/* treat the remaining string as the arguments,
		 * which may need further parsing
		 */
		char *args = cmd + strlen(cmd) + 1;
		if(args >= str_end) {
			args = NULL;
		}

#ifdef HAS_DEVICE
		extern void sdl_clear_event_queue(void);
		sdl_clear_event_queue();
#endif

		int i;
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(cmd, cmd_table[i].name) == 0) {
				if(cmd_table[i].handler(args) < 0) { return; }
				break;
			}
		}

		if(i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
	}
}
