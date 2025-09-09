#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>

enum {
	NOTYPE = 256, EQ,

	/* TODO: Add more token types */
	NEQ, NUM, OR, AND, REG, ID, REF, NEG,
};

static struct rule {
	char *regex;
	int token_type;
} rules[] = {

	/* TODO: Add more rules.
	 * Pay attention to the precedence level of different rules.
	 */

	{" +",	NOTYPE},				// spaces
	{"\\+", '+'},					// plus
	{"-", '-'},						// subtraction
	{"\\*", '*'},					// multiplication
	{"/", '/'},						// division
	{"==", EQ},						// equal
	{"!=", NEQ},					// not equal
	{"\\&\\&", AND},				// and
	{"\\|\\|", OR},					// or
	{"\\!", '!'},
	{"0x[0-9a-fA-F]{1,8}", NUM},	// HEX
	{"[0-9]{1,10}", NUM},		// DEC
	{"\\$[a-z]{1,31}", REG},		// register name
	{"[a-zA-Z_]{1,31}", ID},		// identifiers
	{"\\(", '('},					// left bracket
	{"\\)", ')'},					// right bracket
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
	int i;
	char error_msg[128];
	int ret;

	for(i = 0; i < NR_REGEX; i ++) {
		// 编译正则表达式
		ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
		if(ret != 0) {
			regerror(ret, &re[i], error_msg, 128);
			Assert(ret == 0, "regex compilation failed: %s\n%s", error_msg, rules[i].regex);
		}
	}
}

typedef struct token {
	int type;
	char str[32];
} Token;

Token tokens[32];
int nr_token;

static bool make_token(char *e) {
	int position = 0;
	int i;
	regmatch_t pmatch;
	
	nr_token = 0;

	while(e[position] != '\0') {
		/* Try all rules one by one. */
		for(i = 0; i < NR_REGEX; i ++) {
			if(regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
				char *substr_start = e + position;
				int substr_len = pmatch.rm_eo;

				Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i, rules[i].regex, position, substr_len, substr_len, substr_start);
				position += substr_len;

				/* TODO: Now a new token is recognized with rules[i]. Add codes
				 * to record the token in the array `tokens'. For certain types
				 * of tokens, some extra actions should be performed.
				 */

				switch(rules[i].token_type) {
					case NOTYPE: break;
					// 除了记录 token 类型，还需把字符串存储起来
					// 断言长度不超过 str 数组存储上限
					case NUM: 
					case ID: 
					case REG:
						Assert(substr_len < 32, "length of int is too long (> 31)");
						strncpy(tokens[nr_token].str, substr_start, substr_len);
						tokens[nr_token].str[substr_len] = '\0';
						// sprintf(tokens[nr_token].str, "%.*s", substr_len, substr_start);
					default: 
						tokens[nr_token++].type = rules[i].token_type;
				}
				break;
			}
		}

		if(i == NR_REGEX) {
			printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
			return false;
		}
	}
	return true; 
}

// 判断表达式是否被一对匹配的括号包围，同时检查表达式的括号是否合法
bool check_parentheses(int p,int q) {
	// 首先判断是否最外侧是一对括号
	if(!(tokens[p].type == '(' && tokens[q].type == ')')) return false;
	// 判断括号序列是否合法
	int dlt = 0;
	int i;
	for(i = p; i <= q; ++i) {
		if(tokens[i].type == '(') dlt++;
		if(tokens[i].type == ')') dlt--;
		// 括号序列不合法时，dlt < 0
		assert(dlt >= 0);
	}
	// dlt != 0 时不合法
	assert(dlt == 0);
	return true;
}

int get_op_priority(int op) {
	switch(op) {
		case '!': case NEG: case REF: return 0;
		case '*': case '/': return 1;
		case '+': case '-': return 2;
		case EQ: case NEQ: return 4;
		case AND: return 9;
		case OR: return 10;
		default: assert(0);
	}
}

int find_dominant_operator(int p,int q) {
	int dlt = 0;
	int i;
	int mx_priority = -1, mx_pos = -1;
	for(i = p; i <= q; ++i) {
		switch(tokens[i].type) {
			case NUM: case REG: case ID: break;
			case '(': dlt++; break;
			case ')': dlt--; break;
			default:
				if(dlt == 0) {
					int now_priority = get_op_priority(tokens[i].type);
					// 仅当只存在单目运算符时，才会将其判断为主运算符
					if(now_priority > mx_priority ||  
						(now_priority == mx_priority 
						&& tokens[i].type != '!' && 
						tokens[i].type != NEG && tokens[i].type != REF)) {
						mx_priority = now_priority, mx_pos = i;
					}
				}
				break;
		}
	}
	assert(mx_pos != -1);
	return mx_pos;
}

uint32_t get_reg_val(const char *s);

uint32_t eval(int p,int q,bool *success) {
	// printf("%d %d\n",p,q);
	if(p > q) {
		// 表达式异常
		// printf("%d %d\n",p,q);
		assert(0);
	}
	else if(p == q) {
		uint32_t val;
		switch(tokens[p].type) {
			// 去除寄存器前的 $
			case REG: 
				val = get_reg_val(tokens[p].str + 1);
				break;
			// 自动按照进制转换
			case NUM: 
				val = strtol(tokens[p].str, NULL, 0);
				break;
			// 按变量名查找暂时不实现
			// case ID:
			default:
				assert(0);
		} 
		*success = true;
		return val;
	}
	else if(check_parentheses(p,q) == true) {
		// 表达式被括号包围，
		// 此时去掉最外层括号，表达式不变
		return eval(p+1,q-1,success);
	}
	else {
		int op = find_dominant_operator(p,q);
		int op_type = tokens[op].type;
		// printf("Domi op: %d %d\n",op,op_type);
		// 单目运算符
		if(op_type == '!' || op_type == NEG || op_type == REF) {
			uint32_t val = eval(op+1,q,success);
			switch(op_type) {
				case '!': 
					return !val;
				case NEG: 
					return -val;
				case REF: 
					// current_sreg = R_DS;  // 暂时注释掉，但保留以供将来使用
					return swaddr_read(val, 4);
				
				default: assert(0);
			}
		}
		uint32_t Lval = eval(p,op-1,success);
		uint32_t Rval = eval(op+1,q,success);
		switch(op_type) {
			case '+': return Lval + Rval;
			case '-': return Lval - Rval;
			case '*': return Lval * Rval;
			case '/': return Lval / Rval;
			case EQ: return Lval == Rval;
			case NEQ: return Lval != Rval;
			case AND: return Lval && Rval;
			case OR: return Lval || Rval;
			default: assert(0);
		}
	}
}

uint32_t expr(char *e, bool *success) {
	if(!make_token(e)) {
		*success = false;
		return 0;
	}
	/* TODO: Insert codes to evaluate the expression. */
	// panic("please implement me");
	/* 寻找 NEG 和 REF 的 tokens */
	int i;
	int prev_type;
	for(i = 0; i < nr_token; ++i) {
		// 判断 NEG
		if(tokens[i].type == '-') {
			if(i == 0) {
				tokens[i].type = NEG;
				continue;
			}
			prev_type = tokens[i - 1].type;
			if(!(prev_type == ')' || prev_type == ID || prev_type == NUM ||
			prev_type == REG)) {
				tokens[i].type = NEG;
			}
		}
		// 判断 REF
		else if(tokens[i].type == '*') {
			if(i == 0) {
				tokens[i].type = REF;
				continue;
			}
			prev_type = tokens[i - 1].type;
			if(!(prev_type == ')' || prev_type == ID || prev_type == NUM ||
			prev_type == REG)) {
				tokens[i].type = REF;
			}
		}
	}
	return eval(0,nr_token-1,success);
}

