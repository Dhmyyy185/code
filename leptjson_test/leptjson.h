#ifndef LEPTJSON_H__//定义宏防止多次引入头文件
#define LEPTJSON_H__

#include <stddef.h>
//枚举可能会出现的7中数据类型
typedef enum { LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT } lept_type;

#define LEPT_KEY_NOT_EXIST ((size_t)-1)

typedef struct lept_value lept_value;//前向声明
typedef struct lept_member lept_member;

//只需要实现null、true、false的解析，所以只需要存储一个lept_type枚举类型
struct lept_value {
    union {
        struct { lept_member* m; size_t size, capacity; }o;/* object: members, member count, capacity */
        struct { lept_value* e; size_t size, capacity; }a;/* array:  elements, element count(当前有效元素个数), capacity(当前分配元素个数) */
        struct { char* s; size_t len; }s; /*string*/
        double n; /*number*/
    }u;
    lept_type type;
};

struct lept_member {
    char* k; size_t klen;   /* member key string, key string length */
    lept_value v;           /* member value */
};

//返回值为以下枚举值
enum {
    LEPT_PARSE_OK = 0,//无错误返回该值
    LEPT_PARSE_EXPECT_VALUE,//JSON只含有空白
    LEPT_PARSE_INVALID_VALUE,//值不是那三种字面值
    LEPT_PARSE_ROOT_NOT_SINGULAR,//一个值后，在空白之后还有其他字符
    LEPT_PARSE_NUMBER_TOO_BIG,//数字值太大
    LEPT_PARSE_MISS_QUOTATION_MARK,//引号不全
    LEPT_PARSE_INVALID_STRING_ESCAPE,//不合法的转义字符
    LEPT_PARSE_INVALID_STRING_CHAR,//不合法的字符
    LEPT_PARSE_INVALID_UNICODE_HEX,//不合法的16进制数
    LEPT_PARSE_INVALID_UNICODE_SURROGATE,//不合法的代理对
    LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET,//逗号或中括号不合法
    LEPT_PARSE_MISS_KEY,
    LEPT_PARSE_MISS_COLON,
    LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET,
    LEPT_STRINGIFY_OK
};

#define lept_init(v) do { (v)->type = LEPT_NULL; } while(0)//调用所有访问函数前，先初始化该类型

int lept_parse(lept_value* v, const char* json);//解析JSON
char* lept_stringify(const lept_value* v, size_t* length);//把树形结构转换为JSON文本

void lept_free(lept_value* v);

lept_type lept_get_type(const lept_value* v);//访问结果的函数,就是获取其类型

#define lept_set_null(v) lept_free(v)

int lept_get_boolean(const lept_value* v);//获取JSON(false、true)的值
void lept_set_boolean(lept_value* v, int b);//写入JSON类型值

double lept_get_number(const lept_value* v);//获取JSON数字的数值
void lept_set_number(lept_value* v, double n);//写入JSON数字

const char* lept_get_string(const lept_value* v);//获取JSON字符串
size_t lept_get_string_length(const lept_value* v);//获取JSON字符串的长度
void lept_set_string(lept_value* v, const char* s, size_t len);//写入JSON字符串

void lept_set_array(lept_value* v, size_t capacity);//提供初始数组容量
size_t lept_get_array_size(const lept_value* v);//获取数组长度
size_t lept_get_array_capacity(const lept_value* v);//获取当前容量
void lept_reserve_array(lept_value* v, size_t capacity);//扩大容量
void lept_shrink_array(lept_value* v);//容量缩小
void lept_clear_array(lept_value* v);//清除所有元素(不改容量)
lept_value* lept_get_array_element(const lept_value* v, size_t index);//获取数组元素
lept_value* lept_pushback_array_element(lept_value* v);//数组末端压入一个元素，返回新的元素指针
void lept_popback_array_element(lept_value* v);//弹出数据
lept_value* lept_insert_array_element(lept_value* v, size_t index);//在index位置插入一个元素
void lept_erase_array_element(lept_value* v, size_t index, size_t count);//删去在index位置开始共count个元素(不改容量)

//member = string ws % x3A ws value
//object = % x7B ws[member * (ws % x2C ws member)] ws % x7D
void lept_set_object(lept_value* v, size_t capacity);//提供初始对象容量
size_t lept_get_object_size(const lept_value* v);//获取解析对象长度
size_t lept_get_object_capacity(const lept_value* v);//获取当前容量
void lept_reserve_object(lept_value* v, size_t capacity);//扩大容量
void lept_shrink_object(lept_value* v);//容量缩小
void lept_clear_object(lept_value* v);//清除所有元素(不改容量)
const char* lept_get_object_key(const lept_value* v, size_t index);//获取解析对象键值
size_t lept_get_object_key_length(const lept_value* v, size_t index);//获取解析对象键值长度
lept_value* lept_get_object_value(const lept_value* v, size_t index);//获取解析对象键值后的值
size_t lept_find_object_index(const lept_value* v, const char* key, size_t klen);//简单线性查找JSON对象中某个键是否存在
lept_value* lept_find_object_value(const lept_value* v, const char* key, size_t klen);//获取键对应的值
lept_value* lept_set_object_value(lept_value* v, const char* key, size_t klen);/* 返回新增键值对的指针 */
void lept_remove_object_value(lept_value* v, size_t index);

int lept_is_equal(const lept_value* lhs, const lept_value* rhs);//相等比较
void lept_copy(lept_value* dst, const lept_value* src);//深度复制
void lept_move(lept_value* dst, lept_value* src);//移动语意
void lept_swap(lept_value* lhs, lept_value* rhs);//交换值接口

#endif /* LEPTJSON_H__ */