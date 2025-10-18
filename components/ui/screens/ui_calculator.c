#include "../ui.h"
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h> // 用于sprintf
#define MAX_STACK 100
#define MAX_EXPR_LEN 100
lv_obj_t *ui_calculator_screen=NULL;
lv_obj_t *ui_cal_display_label=NULL;
// 栈结构（用于解析器）
typedef struct
{
    double data[MAX_STACK];
    int top; //stack top pointer
} StackDouble;
typedef struct
{
    char data[MAX_STACK];
    int top; //stack top pointer
} StackChar;
// 栈操作
void stack_double_init(StackDouble *s) { s->top = -1; }
void stack_char_init(StackChar *s) { s->top = -1; }
void stack_double_push(StackDouble *s, double val) {
    if (s->top < MAX_STACK - 1) s->data[++s->top] = val;
}
double stack_double_pop(StackDouble *s) {
    if (s->top >= 0) return s->data[s->top--];
    return 0; // 错误处理简化
}
void stack_char_push(StackChar *s, char val) {
    if (s->top < MAX_STACK - 1) s->data[++s->top] = val;
}
char stack_char_pop(StackChar *s) {
    if (s->top >= 0) return s->data[s->top--];
    return 0;
}
// 运算符是否触发计算（优先级判断）
int operator_causes_evaluation(char op, char prev_op) {
    if (op == '+' || op == '-') return prev_op != '(';
    if (op == '*' || op == '/') return prev_op == '*' || prev_op == '/';
    if (op == ')') return 1;
    return 0;
}
// 执行运算
void execute_operation(StackDouble *v_stack, StackChar *op_stack) {
    double right = stack_double_pop(v_stack);
    double left = stack_double_pop(v_stack);
    char op = stack_char_pop(op_stack);
    double result = 0;
    switch (op) {
        case '+': result = left + right; break;
        case '-': result = left - right; break;
        case '*': result = left * right; break;
        case '/': 
            if (right == 0) {
                result = NAN;  // 处理除零
            } else {
                result = left / right;
            }
            break;
    }
    stack_double_push(v_stack, result);
}
// 处理闭括号（虽无括号，但保留以兼容）
void process_closing_parenthesis(StackDouble *v_stack, StackChar *op_stack) {
    while (op_stack->top >= 0 && op_stack->data[op_stack->top] != '(') {
        execute_operation(v_stack, op_stack);
    }
    if (op_stack->top >= 0) stack_char_pop(op_stack);
}
// 处理数字（支持小数点）
int process_input_number(const char *exp, int pos, StackDouble *v_stack) {
    double value = 0.0;
    int decimal_place = 0;
    int in_decimal = 0;
    int len = strlen(exp);
    while (pos < len && (isdigit((unsigned char)exp[pos]) || exp[pos] == '.')) {
        if (exp[pos] == '.') {
            if (in_decimal) break; // 多个.错误
            in_decimal = 1;
            pos++;
            continue;
        }
        value = 10 * value + (exp[pos] - '0');
        if (in_decimal) decimal_place++;
        pos++;
    }
    while (decimal_place > 0) {
        value /= 10.0;
        decimal_place--;
    }
    stack_double_push(v_stack, value);
    return pos;
}
// 处理运算符
void process_input_operator(char op, StackDouble *v_stack, StackChar *op_stack) {
    while (op_stack->top >= 0 && operator_causes_evaluation(op, op_stack->data[op_stack->top])) {
        execute_operation(v_stack, op_stack);
    }
    stack_char_push(op_stack, op);
}
// calculate expression results
double evaluate_expression(const char *exp)
{
    StackDouble v_stack;
    StackChar op_stack;
    stack_double_init(&v_stack);
    stack_char_init(&op_stack);
    stack_char_push(&op_stack, '('); // 隐式括号
    int pos = 0;
    int n = strlen(exp);
    bool last_was_value = false;
    while (pos <= n)
    {
        if (pos == n || exp[pos] == ')')
        {
            process_closing_parenthesis(&v_stack, &op_stack);
            pos++;
            last_was_value = true;
        } else if (exp[pos] == '(') {
            stack_char_push(&op_stack, '(');
            pos++;
            last_was_value = false;
        } else if (isdigit((unsigned char)exp[pos]) || (exp[pos] == '.' && pos + 1 < n && isdigit((unsigned char)exp[pos + 1])))
        {
            pos = process_input_number(exp, pos, &v_stack);
            last_was_value = true;
        }
        //operate code
        else if (strchr("+-*/", exp[pos]))
        {
            char op = exp[pos];
            if (op == '-' && !last_was_value) {
                stack_double_push(&v_stack, 0.0);
            }
            process_input_operator(op, &v_stack, &op_stack);
            pos++;
            last_was_value = false;
        } else
        {
            pos++; // 忽略无效
            // 不改变 last_was_value
        }
    }
    return stack_double_pop(&v_stack);
}
// LVGL 事件处理
static char expr_buffer[MAX_EXPR_LEN] = "";
static void btn_event_handler(lv_event_t *e) {
    lv_obj_t *btnm = lv_event_get_target(e);
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED)
    {
        uint32_t id = lv_btnmatrix_get_selected_btn(btnm);
        const char *txt = lv_btnmatrix_get_btn_text(btnm, id);
        //click all clear button then clear expression array
        if (strcmp(txt, "ac") == 0)
        {
            expr_buffer[0] = '\0';
        }
        //click delete button then clear last input number
        else if (strcmp(txt, "del") == 0)
        {
            size_t len = strlen(expr_buffer);
            if (len > 0) expr_buffer[len - 1] = '\0';
        }
        //calculate espression results
        else if (strcmp(txt, "=") == 0)
        {
            double result = evaluate_expression(expr_buffer);
            if (isnan(result) || isinf(result)) {
                expr_buffer[0] = '\0';  // 丢弃整个表达式（除零或无穷）
            } else {
                snprintf(expr_buffer, MAX_EXPR_LEN, "%.2f", result); // 保留2位小数
            }
        }
        //in this case,input is number(0-9) / (+ - * /)
        else
        {
            strncat(expr_buffer, txt, MAX_EXPR_LEN - strlen(expr_buffer) - 1);
        }
        // update label
        lv_label_set_text(ui_cal_display_label, expr_buffer);
    }
}
// 创建UI
void ui_calculator_screen_init(void)
{
    ui_calculator_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_calculator_screen, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                             LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_calculator_screen, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_calculator_screen, 255, LV_PART_MAIN);
    lv_obj_remove_style_all(ui_calculator_screen);
   
   
    // 显示标签
    ui_cal_display_label = lv_label_create(ui_calculator_screen);
    lv_label_set_text(ui_cal_display_label, "");
    lv_label_set_long_mode(ui_cal_display_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_size(ui_cal_display_label, 240, 60);
    lv_obj_align(ui_cal_display_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_align(ui_cal_display_label, LV_TEXT_ALIGN_RIGHT, 0);
    // 按钮地图
    static const char *btn_map[] = {
        "(", ")", "ac", "del", "\n",
        "7", "8", "9", "/", "\n",
        "4", "5", "6", "*", "\n",
        "1", "2", "3", "-", "\n",
        "0", ".", "=", "+", ""
    };
    lv_obj_t *btnm = lv_btnmatrix_create(ui_calculator_screen);
    lv_btnmatrix_set_map(btnm, btn_map);
    lv_obj_set_size(btnm, 240, 260);
    lv_obj_align(btnm, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(btnm, btn_event_handler, LV_EVENT_ALL, NULL);
}
// 在main或init中调用：lv_init()等后，calculator_create();