#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

#define MAXSTACKSIZE    100
#define OKAY            1
#define ERROR  	        0

static char ops[7] = {'+','-','*','/','(',')','#'};
static int cmp[7][7] = {
    {2,2,1,1,1,2,2},
    {2,2,1,1,1,2,2},
    {2,2,2,2,1,2,2},
    {2,2,2,2,1,2,2},
    {1,1,1,1,1,3,0},
    {2,2,2,2,0,2,2},
    {1,1,1,1,1,0,3},
};
 
struct stack_op {
    char op[MAXSTACKSIZE];
    int top;
};
struct stack_num {
    double num[MAXSTACKSIZE];
    int top;
};

static int push_stackop(struct stack_op *s ,char c)
{
    if(s->top == MAXSTACKSIZE - 1)
        return ERROR;
    s->top++;
    s->op[s->top] = c;
    return OKAY; 	
}

static int push_stack_num(struct stack_num *s,double n)
{
    if(s->top == MAXSTACKSIZE - 1)
        return ERROR;
    s->top++;
    s->num[s->top] = n;
    return OKAY; 
}

static int isempty_stackop(struct stack_op *s)
{
    return ((s->top == -1) ? ERROR : OKAY);
}

static int isempty_stacknum(struct stack_num *s)
{
    return ((s->top == -1) ? ERROR : OKAY);
}

static char pop_stackop(struct stack_op *s)
{
    char c = 0;
    if(s->top == -1) {
        // printf("stackop empty!\n");
        return ERROR;
    }
    c = s->op[s->top];
    s->top--;
    return c;
}

static double pop_stacknum(struct stack_num *s)
{
    double n = 0;
    if(s->top == -1) {
        // printf("stacknum empty!\n");
        return ERROR;
    }
    n = s->num[s->top];
    s->top--;
    return n;
}

static char get_stackop_top(struct stack_op *s)
{
    if(s->top == -1) {
        // printf("stackop empty!\n");
        return ERROR;
    }
    return (s->op[s->top]);
}

static double get_stacknum_top(struct stack_num *s)
{
    if(s->top == -1) {
        // printf("stacknum empty!\n");
        return ERROR;
    }
    return (s->num[s->top]);
}

static char compare(char op1,char op2)
{
    int i, m = 0 ,n = 0, priority;
    char pri = 0;
    
    for(i = 0;i<7;i++) {
        if(op1 == ops[i]) {
            m = i;
        }
        if(op2 == ops[i]) {
            n = i;
        }
    }
    priority = cmp[m][n];
    switch(priority) {
    case 1:
        pri = '<';
        break;
    case 2:
        pri = '>';
        break;
    case 3:
        pri = '=';
        break;
    case 0:
        pri = '$';
        // printf("expression error!\n");
        break; 
    }
    return pri;
}

static double compute(double a,char op,double b)
{
    double result = 0;
    switch(op) {
    case '+': result = a + b; break;
    case '-': result = a - b; break;
    case '*': result = a * b; break;
    case '/': result = a / b; break;
    }
    return result;
} 
/* 注意： 不接受空格 */
double calculate(char *expression, const uint32_t len)
{
    double a = 0, b = 0, temp = 0, sum = 0;
    char op, num_buf[32] = {0};
    char *ch = NULL, *buf = NULL;
    struct stack_op sop        = {{0}, -1};
    struct stack_num snum      = {{0}, -1};
    struct stack_op *sop_ptr   = &sop;
    struct stack_num *snum_ptr = &snum;

    buf = (char *)malloc(len);
    strcpy(buf, expression);
    /* 在表达式后面添加'#' */
    ch = buf;
    ch += strlen(buf);
    *ch++ = '#';
    *ch = 0;
    ch = buf;

    push_stackop(sop_ptr,'#');
    while(*ch != '#' || get_stackop_top(sop_ptr) != '#') {
        char *p = num_buf;
        if (*ch >= '0' && *ch <= '9') {
            while ((*ch >= '0' && *ch <= '9') || *ch == '.') { *p++ = *ch++; }
            *p = 0;
            temp = atof(num_buf);
            // printf("temp:%lf\n", temp);
            push_stack_num(snum_ptr, temp);
        }
        else {
            switch(compare(get_stackop_top(sop_ptr), *ch)) {
            case'<':
                push_stackop(sop_ptr,*ch);
                ch++;
                break;
            case'=':
                pop_stackop(sop_ptr);
                ch++;
                break; 
            case'>':
                op = pop_stackop(sop_ptr);
                b = pop_stacknum(snum_ptr);
                a = pop_stacknum(snum_ptr);
                sum = compute(a,op,b);
                push_stack_num(snum_ptr,sum);
                break;
            }
        }
    }
    // printf("%lf", get_stacknum_top(snum_ptr));
    free(buf);
    return get_stacknum_top(snum_ptr);
}

#if 0
int main(void)
{
    char buf[] = "-005";
    double ret = 0;
    ret = calculate(buf, sizeof(buf));
    printf("%lf\n", ret);
}
#endif