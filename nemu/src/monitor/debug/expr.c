#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>

enum {
	NOTYPE = 256, EQ, NEQ, ANDAND, OROR, SHL, SHR,
	TK_NUM, TK_HEX, TK_REG,
};

static struct rule {
	char *regex;
	int token_type;
} rules[] = {

	/* Pay attention to the precedence level of different rules.
	 * Multi-character operators must be placed before their
	 * single-character prefixes, otherwise they would be split
	 * into two tokens (e.g. "!=" would become "!" and "=").
	 */

	{" +",		NOTYPE},				// spaces
	{"\\+",		'+'},					// plus
	{"==",		EQ},					// equal
	{"!=",		NEQ},					// not equal
	{"&&",		ANDAND},				// logical and
	{"\\|\\|",	OROR},					// logical or
	{"<<",		SHL},					// shift left
	{">>",		SHR},					// shift right
	{"\\(",		'('},					// left parenthesis
	{"\\)",		')'},					// right parenthesis
	{"\\*",		'*'},					// multiply / dereference
	{"\\/",		'/'},					// divide
	{"\\-",		'-'},					// minus / negative
	{"\\&",		'&'},					// bitwise and
	{"\\|",		'|'},					// bitwise or
	{"\\^",		'^'},					// bitwise xor
	{"!",		'!'},					// logical not
	{"0x[0-9a-fA-F]+",	TK_HEX},		// hexadecimal number
	{"[0-9]+",	TK_NUM},				// decimal number
	{"\\$[a-z]+",	TK_REG},			// register access, e.g. $eax
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
	int i;
	char error_msg[128];
	int ret;

	for(i = 0; i < NR_REGEX; i ++) {
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

				/* Record the token. Spaces (NOTYPE) are discarded. */
				if(rules[i].token_type != NOTYPE) {
					Assert(nr_token < 32, "too many tokens in expression");
					tokens[nr_token].type = rules[i].token_type;

					/* For tokens that carry a value (numbers, registers),
					 * record the corresponding substring. The buffer is
					 * limited, so guard against overflow.
					 */
					if(rules[i].token_type == TK_NUM ||
					   rules[i].token_type == TK_HEX ||
					   rules[i].token_type == TK_REG) {
						Assert(substr_len < 32, "token too long: %.*s", substr_len, substr_start);
						strncpy(tokens[nr_token].str, substr_start, substr_len);
						tokens[nr_token].str[substr_len] = '\0';
					}
					nr_token ++;
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

/* Return the value of a single token: a number, a hex number, or a register. */
static uint32_t get_token_value(int i, bool *success) {
	if(tokens[i].type == TK_NUM) {
		return (uint32_t)atol(tokens[i].str);
	}
	if(tokens[i].type == TK_HEX) {
		return (uint32_t)strtoul(tokens[i].str, NULL, 16);
	}
	if(tokens[i].type == TK_REG) {
		/* The token string is like "$eax"; skip the leading '$'. */
		char *name = tokens[i].str + 1;
		int j;
		if(strcmp(name, "eip") == 0) { return cpu.eip; }
		for(j = 0; j < 8; j ++) {
			if(strcmp(name, regsl[j]) == 0) { return cpu.gpr[j]._32; }
		}
		for(j = 0; j < 8; j ++) {
			if(strcmp(name, regsw[j]) == 0) { return cpu.gpr[j]._16; }
		}
		for(j = 0; j < 8; j ++) {
			if(strcmp(name, regsb[j]) == 0) { return cpu.gpr[j & 0x3]._8[j >> 2]; }
		}
		*success = false;
		return 0;
	}
	*success = false;
	return 0;
}

/* Is this token an operand (i.e. something that can be the left side of a binary op)? */
static bool is_operand(int type) {
	return type == TK_NUM || type == TK_HEX || type == TK_REG || type == ')';
}

/* Is this token a unary operator? */
static bool is_unary(int type) {
	return type == '!' || type == '-' || type == '*';
}

/* Precedence of a binary operator. Higher value binds tighter.
 * The dominant operator is the one with the lowest precedence.
 * Return 0 if the token is not a binary operator.
 */
static int get_precedence(int type) {
	switch(type) {
		case OROR:	return 1;
		case ANDAND:	return 2;
		case '|':	return 3;
		case '^':	return 4;
		case '&':	return 5;
		case EQ:	case NEQ:	return 6;
		case SHL:	case SHR:	return 7;
		case '+':	case '-':	return 8;
		case '*':	case '/':	return 9;
		default:	return 0;
	}
}

/* Check whether the sub-expression [p, q] is surrounded by a matched
 * pair of parentheses. If so, return true.
 */
static bool check_parentheses(int p, int q) {
	if(tokens[p].type != '(' || tokens[q].type != ')') { return false; }
	int cnt = 0;
	int i;
	for(i = p; i <= q; i ++) {
		if(tokens[i].type == '(') { cnt ++; }
		else if(tokens[i].type == ')') {
			cnt --;
			if(cnt < 0) { return false; }
			if(cnt == 0 && i != q) { return false; }
		}
	}
	return cnt == 0;
}

/* Find the dominant (lowest-precedence, rightmost) binary operator in [p, q].
 * Return its index, or -1 if there is none.
 */
static int find_dominant(int p, int q) {
	int op = -1;
	int op_prec = 0;
	int cnt = 0;
	int i;
	for(i = p; i <= q; i ++) {
		int t = tokens[i].type;
		if(t == '(') { cnt ++; }
		else if(t == ')') { cnt --; }
		else if(cnt == 0) {
			int prec = get_precedence(t);
			if(prec > 0) {
				/* Only a binary operator (one that has a left operand)
				 * can be a dominant operator.
				 */
				if(i > p && is_operand(tokens[i - 1].type)) {
					if(op == -1 || prec <= op_prec) {
						op_prec = prec;
						op = i;
					}
				}
			}
		}
	}
	return op;
}

static uint32_t eval(int p, int q, bool *success) {
	if(p > q) {
		*success = false;
		return 0;
	}

	/* A single token: a number, a hex number, or a register. */
	if(p == q) {
		return get_token_value(p, success);
	}

	/* The whole sub-expression is surrounded by a matched pair of parentheses. */
	if(check_parentheses(p, q)) {
		return eval(p + 1, q - 1, success);
	}

	/* Find the dominant binary operator. */
	int op = find_dominant(p, q);

	if(op == -1) {
		/* No binary operator. If the first token is a unary operator,
		 * it applies to the rest of the sub-expression.
		 */
		if(is_unary(tokens[p].type)) {
			uint32_t val = eval(p + 1, q, success);
			if(!*success) { return 0; }
			switch(tokens[p].type) {
				case '!':	return !val;
				case '-':	return -val;
				case '*':	return swaddr_read(val, 4);	/* dereference */
			}
		}
		*success = false;
		return 0;
	}

	uint32_t val1 = eval(p, op - 1, success);
	if(!*success) { return 0; }
	uint32_t val2 = eval(op + 1, q, success);
	if(!*success) { return 0; }

	switch(tokens[op].type) {
		case '+':	return val1 + val2;
		case '-':	return val1 - val2;
		case '*':	return val1 * val2;
		case '/':	return val1 / val2;
		case EQ:	return val1 == val2;
		case NEQ:	return val1 != val2;
		case ANDAND:	return val1 && val2;
		case OROR:	return val1 || val2;
		case '&':	return val1 & val2;
		case '|':	return val1 | val2;
		case '^':	return val1 ^ val2;
		case SHL:	return val1 << val2;
		case SHR:	return val1 >> val2;
	}

	*success = false;
	return 0;
}

uint32_t expr(char *e, bool *success) {
	if(!make_token(e)) {
		*success = false;
		return 0;
	}

	return eval(0, nr_token - 1, success);
}
