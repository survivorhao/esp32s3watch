#include "../ui.h"
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h> 



#define MAX_STACK 100
#define MAX_EXPR_LEN 100
lv_obj_t *ui_calculator_screen=NULL;
lv_obj_t *ui_cal_display_label=NULL;
lv_obj_t *ui_cal_exit_but=NULL;

//store input expression
static char expr_buffer[MAX_EXPR_LEN] = "";

//value stack, store number 
typedef struct
{
    double data[MAX_STACK];
    int top;                //stack top pointer
} StackDouble;


//store operator 
typedef struct
{
    char data[MAX_STACK];
    int top;                //stack top pointer
} StackChar;



void stack_double_init(StackDouble *s) { s->top = -1; }
void stack_char_init(StackChar *s) { s->top = -1; }
void stack_double_push(StackDouble *s, double val) 
{
    if (s->top < MAX_STACK - 1) s->data[++s->top] = val;
}
double stack_double_pop(StackDouble *s) 
{
    if (s->top >= 0) return s->data[s->top--];
    return 0; 
}
void stack_char_push(StackChar *s, char val) 
{
    if (s->top < MAX_STACK - 1) s->data[++s->top] = val;
}
char stack_char_pop(StackChar *s) 
{
    if (s->top >= 0) return s->data[s->top--];
    return 0;
}


/// @brief judge current operate code and stack top operate code
/// @param op   current
/// @param prev_op  stack top 
/// @return whether to calculate
int operator_causes_evaluation(char op, char prev_op) 
{
    if (op == '+' || op == '-') return prev_op != '(';
    if (op == '*' || op == '/') return prev_op == '*' || prev_op == '/';
    if (op == ')') return 1;
    return 0;
}


/// @brief 
/// @param v_stack 
/// @param op_stack 
void execute_operation(StackDouble *v_stack, StackChar *op_stack) 
{
    double right = stack_double_pop(v_stack);
    double left = stack_double_pop(v_stack);
    char op = stack_char_pop(op_stack);
    double result = 0;
    switch (op) {
        case '+': result = left + right; break;
        case '-': result = left - right; break;
        case '*': result = left * right; break;
        case '/': 
            //judge whether right equal 0
            if (right == 0) 
            {
                result = NAN;  
            } 
            else 
            {
                result = left / right;
            }
            break;
    }
    stack_double_push(v_stack, result);
}
//process ')'
void process_closing_parenthesis(StackDouble *v_stack, StackChar *op_stack) 
{
    while (op_stack->top >= 0 && op_stack->data[op_stack->top] != '(') 
    {
        execute_operation(v_stack, op_stack);
    }

    //pop '('
    if (op_stack->top >= 0) stack_char_pop(op_stack);
}


//process input is number (0-9)
int process_input_number(const char *exp, int pos, StackDouble *v_stack) {
    double value = 0.0;
    int decimal_place = 0;
    int in_decimal = 0;
    int len = strlen(exp);
    while (pos < len && (isdigit((unsigned char)exp[pos]) || exp[pos] == '.')) 
    {
        if (exp[pos] == '.') 
        {
            if (in_decimal) break; //many . , this is illegal situation 
            in_decimal = 1;
            pos++;
            continue;
        }
        value = 10 * value + (exp[pos] - '0');
        
        //record decimal count
        if (in_decimal) decimal_place++;
        pos++;
    }
    //int -> double
    while (decimal_place > 0) 
    {
        value /= 10.0;
        decimal_place--;
    }
    stack_double_push(v_stack, value);
    return pos;
}
// process input is opeartor
void process_input_operator(char op, StackDouble *v_stack, StackChar *op_stack) 
{
    while (op_stack->top >= 0 && operator_causes_evaluation(op, op_stack->data[op_stack->top])) 
    {
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
        if(pos == n || exp[pos] == ')')
        {
            process_closing_parenthesis(&v_stack, &op_stack);
            pos++;
            last_was_value = true;
        }else if (exp[pos] == '(') 
        {
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
            //consider -5+10 similar situation
            if (op == '-' && !last_was_value) 
            {
                stack_double_push(&v_stack, 0.0);
            }
            process_input_operator(op, &v_stack, &op_stack);
            pos++;
            last_was_value = false;
        }else
        {
            //input  is our expected,just skip it 
            pos++;

        }
    }
    return stack_double_pop(&v_stack);
}


static void btn_event_handler(lv_event_t *e) 
{
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

            //when results is not a number or is infinite
            if(isnan(result) || isinf(result)) 
            {
                //discard this results
                expr_buffer[0] = '\0';  
            }else 
            {
                snprintf(expr_buffer, MAX_EXPR_LEN, "%.2f", result); 
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

void ui_calculator_return_event_cb(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    if(event_code == LV_EVENT_CLICKED) 
    {

        _ui_screen_change(&ui_app1, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_app1_screen_init);     
        if(ui_calculator_screen!=NULL)
        {
            lv_obj_del_delayed(ui_calculator_screen,300);
            ui_calculator_screen=NULL;
            
            //clear input expression array
            expr_buffer[0] = '\0';
            
        }
    }
}

//create calculator screen
void ui_calculator_screen_init(void)
{
    ui_calculator_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_calculator_screen, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                             LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_calculator_screen, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_calculator_screen, 255, LV_PART_MAIN);
    lv_obj_remove_style_all(ui_calculator_screen);
   
    ui_cal_exit_but = lv_img_create(ui_calculator_screen);
    lv_img_set_src(ui_cal_exit_but , &ui_img_return_png);
    lv_obj_set_width(ui_cal_exit_but , LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_cal_exit_but , LV_SIZE_CONTENT);    /// 1
    lv_obj_align(ui_cal_exit_but , LV_ALIGN_TOP_MID, -90, -80);
    lv_obj_add_flag(ui_cal_exit_but , LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_clear_flag(ui_cal_exit_but , LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                        LV_OBJ_FLAG_SNAPPABLE  | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                        LV_OBJ_FLAG_SCROLL_CHAIN);     /// Flags
    lv_img_set_zoom(ui_cal_exit_but , 50);
    lv_obj_set_style_bg_color(ui_cal_exit_but , lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_cal_exit_but , 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_img_recolor(ui_cal_exit_but , lv_color_hex(0x000000), LV_PART_MAIN );
    lv_obj_set_style_img_recolor_opa(ui_cal_exit_but , 0, LV_PART_MAIN);

    lv_obj_add_event_cb(ui_cal_exit_but , ui_calculator_return_event_cb, LV_EVENT_CLICKED, NULL);

    //display input expression and results
    ui_cal_display_label = lv_label_create(ui_calculator_screen);
    lv_label_set_text(ui_cal_display_label, "");
    lv_label_set_long_mode(ui_cal_display_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_size(ui_cal_display_label, LV_PCT(100), 60);
    lv_obj_align(ui_cal_display_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_align(ui_cal_display_label, LV_TEXT_ALIGN_RIGHT, 0);

    //
    static const char *btn_map[] = {
        "(", ")", "ac", "del", "\n",
        "7", "8", "9", "/", "\n",
        "4", "5", "6", "*", "\n",
        "1", "2", "3", "-", "\n",
        "0", ".", "=", "+", ""
    };
    lv_obj_t *btnm = lv_btnmatrix_create(ui_calculator_screen);
    lv_btnmatrix_set_map(btnm, btn_map);
    lv_obj_set_size(btnm, LV_PCT(100), 260);
    lv_obj_align(btnm, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(btnm, btn_event_handler, LV_EVENT_ALL, NULL);


}