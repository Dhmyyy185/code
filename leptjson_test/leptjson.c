//内存泄露检测方法
#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

//解析器实现
#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL */
#include <errno.h>   /* errno, ERANGE */
#include <math.h>    /* HUGE_VAL */
#include <ctype.h> /*ISDIGIT*/
#include <stdio.h>

#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#ifndef LEPT_PARSE_STRINGIFY_INIT_SIZE
#define LEPT_PARSE_STRINGIFY_INIT_SIZE 256
#endif

//判断指针c的一个位置是否是ch，并且移动指针
#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch) do { *(char*)lept_context_push(c, sizeof(char)) = (ch); } while(0)
#define PUTS(c, s, len)     memcpy(lept_context_push(c, len), s, len)

//封装JSON的结构体
//设置缓冲区暂存解析的结果，最后再用lept_set_string()把缓冲区的结果设进值
//先进后出方式访问
typedef struct {
    const char* json;
    char* stack;
    size_t size, top;
}lept_context;

//写入时需要释放内存
void lept_free(lept_value* v) { 
    size_t i;
    assert(v != NULL);
    switch (v->type) {
    case LEPT_STRING:
        free(v->u.s.s);
        break;
    case LEPT_ARRAY:
        for (i = 0; i < v->u.a.size; i++)
            lept_free(&v->u.a.e[i]);
        free(v->u.a.e);
        break;
    case LEPT_OBJECT:
        for (i = 0; i < v->u.o.size; i++) {
            free(v->u.o.m[i].k);
            lept_free(&v->u.o.m[i].v);
        }
        free(v->u.o.m);
        break;
    default: break;
    }
    v->type = LEPT_NULL;
}

//获取类型的函数实现
lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

//获取JSON类型值
int lept_get_boolean(const lept_value* v) {
    assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
    return v->type;
}

//写入JSON类型值(false, true)
void lept_set_boolean(lept_value* v, int b) {
    assert(v != NULL);
    lept_free(v);
    v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

//获取JSON数字的数值
double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->u.n;
}

//写入JSON数字值
void lept_set_number(lept_value* v, double n) {
    assert(v != NULL);
    lept_free(v);
    v->u.n = n;
    v->type = LEPT_NUMBER;
}

//获取JSON字符串
const char* lept_get_string(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.s;
}

//获取JSON字符串长度
size_t lept_get_string_length(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.len;
}

//写入JSON字符串
void lept_set_string(lept_value* v, const char* s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    lept_free(v);
    v->u.s.s = (char*)malloc(len + 1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = LEPT_STRING;
}

//提供初始数组容量
void lept_set_array(lept_value* v, size_t capacity) {
    assert(v != NULL);
    lept_free(v);
    v->type = LEPT_ARRAY;
    v->u.a.size = 0;
    v->u.a.capacity = capacity;
    v->u.a.e = capacity > 0 ? (lept_value*)malloc(capacity * sizeof(lept_value)) : NULL;
}

//获取数组长度
size_t lept_get_array_size(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    return v->u.a.size;
}

//获取当前容量的函数
size_t lept_get_array_capacity(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    return v->u.a.capacity;
}

//扩大容量(array)
void lept_reserve_array(lept_value* v, size_t capacity) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    if (v->u.a.capacity < capacity) {
        v->u.a.capacity = capacity;
        v->u.a.e = (lept_value*)realloc(v->u.a.e, capacity * sizeof(lept_value));
    }
}

//容量缩小(array)
void lept_shrink_array(lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    if (v->u.a.capacity > v->u.a.size) {
        v->u.a.capacity = v->u.a.size;
        v->u.a.e = (lept_value*)realloc(v->u.a.e, v->u.a.capacity * sizeof(lept_value));
    }
}

//清除所有元素(不改容量)(array)
void lept_clear_array(lept_value* v) {
    size_t i;
    assert(v != NULL && v->type == LEPT_ARRAY);
    for (i = 0; i < v->u.a.size; i++)
        lept_free(&v->u.a.e[i]);
    for (i = 0; i < v->u.a.size; i++)
        lept_init(&v->u.a.e[i]);
    v->u.a.size = 0;
}

//数组末端压入一个元素，返回新的元素指针
lept_value* lept_pushback_array_element(lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    if (v->u.a.size == v->u.a.capacity)
        lept_reserve_array(v, v->u.a.capacity == 0 ? 1 : v->u.a.capacity * 2);
    lept_init(&v->u.a.e[v->u.a.size]);
    return &v->u.a.e[v->u.a.size++];
}

//弹出数据
void lept_popback_array_element(lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY && v->u.a.size > 0);
    lept_free(&v->u.a.e[--v->u.a.size]);
}

//在index位置插入一个元素
lept_value* lept_insert_array_element(lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_ARRAY && index <= v->u.a.size);
    /*\todo*/
    if (v->u.a.size == v->u.a.capacity) lept_reserve_array(v, v->u.a.capacity == 0 ? 1 : (v->u.a.size << 1));
    memcpy(&v->u.a.e[index + 1], &v->u.a.e[index], (v->u.a.size - index) * sizeof(lept_value));
    lept_init(&v->u.a.e[index]);
    v->u.a.size++;
    return &v->u.a.e[index];
}

//删去在index位置开始共count个元素(不改容量)
void lept_erase_array_element(lept_value* v, size_t index, size_t count) {
    assert(v != NULL && v->type == LEPT_ARRAY && index + count <= v->u.a.size);
    /*\todo*/
    //回收完空间，然后将index后面count个元素移动到index处，最后将空闲的count个元素重新初始化
    size_t i;
    for (i = index; i < index + count; i++)
        lept_free(&v->u.a.e[i]);
    memcpy(v->u.a.e + index, v->u.a.e + index + count, (v->u.a.size - index - count) * sizeof(lept_value));
    for (i = v->u.a.size - count; i < v->u.a.size; i++)
        lept_init(&v->u.a.e[i]);
    v->u.a.size -= count;
}

//获取数组元素
lept_value* lept_get_array_element(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    assert(index < v->u.a.size);
    return &v->u.a.e[index];
}

//提供初始对象容量
void lept_set_object(lept_value* v, size_t capacity) {
    assert(v != NULL);
    lept_free(v);
    v->type = LEPT_OBJECT;
    v->u.o.size = 0;
    v->u.o.capacity = capacity;
    v->u.o.m = capacity > 0 ? (lept_member*)malloc(capacity * sizeof(lept_member)) : NULL;
}

//获取解析对象长度
size_t lept_get_object_size(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    return v->u.o.size;
}

//获取当前容量
size_t lept_get_object_capacity(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    return v->u.o.capacity;
}

//扩大容量(obejct)
void lept_reserve_object(lept_value* v, size_t capacity) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    if (v->u.o.capacity < capacity) {
        v->u.o.capacity = capacity;
        v->u.o.m = (lept_member*)realloc(v->u.o.m, capacity * sizeof(lept_member));
    }
}

//容量缩小(obejct)
void lept_shrink_object(lept_value* v) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    if (v->u.o.capacity > v->u.o.size) {
        v->u.o.capacity = v->u.o.size;
        v->u.o.m = (lept_member*)realloc(v->u.o.m, v->u.o.capacity * sizeof(lept_member));
    }
}

//清除所有元素(不改容量)(obejct)
void lept_clear_object(lept_value* v) {
    size_t i;
    assert(v != NULL && v->type == LEPT_OBJECT);
    //回收k和v空间
    for (i = 0; i < v->u.o.size; i++) {
        free(v->u.o.m[i].k);
        v->u.o.m[i].k = NULL;
        v->u.o.m[i].klen = 0;
        lept_free(&v->u.o.m[i].v);
    }
    v->u.o.size = 0;
}

// 获取解析对象键值
const char* lept_get_object_key(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index < v->u.o.size);
    return v->u.o.m[index].k;
}

//获取解析对象键值长度
size_t lept_get_object_key_length(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index < v->u.o.size);
    return v->u.o.m[index].klen;
}

//获取解析对象键值后的值
lept_value* lept_get_object_value(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index < v->u.o.size);
    return &v->u.o.m[index].v;
}

//简单线性查找JSON对象中某个键是否存在
size_t lept_find_object_index(const lept_value* v, const char* key, size_t klen) {
    size_t i;
    assert(v != NULL && v->type == LEPT_OBJECT && key != NULL);
    for (i = 0; i < v->u.o.size; i++)
        if (v->u.o.m[i].klen == klen && memcmp(v->u.o.m[i].k, key, klen) == 0)
            return i;
    return LEPT_KEY_NOT_EXIST;
}

//辅助函数直接直接获取键对应的值
lept_value* lept_find_object_value(const lept_value* v, const char* key, size_t klen) {
    size_t index = lept_find_object_index(v, key, klen);
    return index != LEPT_KEY_NOT_EXIST ? &v->u.o.m[index].v : NULL;
}

//返回新增键值对的指针
lept_value* lept_set_object_value(lept_value* v, const char* key, size_t klen) {
    assert(v != NULL && v->type == LEPT_OBJECT && key != NULL);
    size_t index, i;
    if ((index = lept_find_object_index(v, key, klen)) != LEPT_KEY_NOT_EXIST)
        return &v->u.o.m[index].v;
    //key not exist,then make room and init
    if (v->u.o.size == v->u.o.capacity) 
        lept_reserve_object(v, v->u.o.capacity == 0 ? 1 : (v->u.o.capacity << 1));
    i = v->u.o.size;
    v->u.o.m[i].k = (char*)malloc((klen + 1));
    memcpy(v->u.o.m[i].k, key, klen);
    v->u.o.m[i].k[klen] = '\0';
    v->u.o.m[i].klen = klen;
    lept_init(&v->u.o.m[i].v);
    v->u.o.size++;
    return &v->u.o.m[i].v;
}

//移除对象中index下标的键
void lept_remove_object_value(lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT && index < v->u.o.size);
    free(v->u.o.m[index].k);
    v->u.o.m[index].klen = 0;
    lept_free(&v->u.o.m[index].v);
    memcpy(&v->u.o.m[index], &v->u.o.m[index + 1], (v->u.o.size - index - 1) * sizeof(lept_member));
    v->u.o.m[--v->u.o.size].k = NULL;
    v->u.o.m[v->u.o.size].klen = 0;
    lept_init(&v->u.o.m[v->u.o.size].v);
}

//相等比较
int lept_is_equal(const lept_value* lhs, const lept_value* rhs) {
    size_t i, j;
    assert(lhs != NULL && rhs != NULL);
    if (lhs->type != rhs->type)
        return 0;
    switch (lhs->type) {
    case LEPT_STRING:
        return lhs->u.s.len == rhs->u.s.len && memcmp(lhs->u.s.s, rhs->u.s.s, lhs->u.s.len) == 0;
    case LEPT_NUMBER:
        return lhs->u.n == rhs->u.n;
    case LEPT_ARRAY:
        if (lhs->u.a.size != rhs->u.a.size)
            return 0;
        for (i = 0; i < lhs->u.a.size; i++)//递归检查数组对应的元素是否相等
            if (!lept_is_equal(&lhs->u.a.e[i], &rhs->u.a.e[i]))
                return 0;
        return 1;
    case LEPT_OBJECT:
        if (lhs->u.o.size != rhs->u.o.size)
            return 0;
        for (i = 0; i < lhs->u.o.size; i++) {//递归检查对象对应的元素是否相等
            if ((j = lept_find_object_index(rhs, lhs->u.o.m[i].k, lhs->u.o.m[i].klen)) == LEPT_KEY_NOT_EXIST) return 0;
            if (!lept_is_equal(&lhs->u.o.m[i].v, &rhs->u.o.m[j].v)) return 0;
        }
        return 1;
    default:
        return 1;
    }
}

//深度复制源JSON(src)
void lept_copy(lept_value* dst, const lept_value* src) {
    size_t i;
    assert(src != NULL && dst != NULL && src != dst);
    switch (src->type) {
    case LEPT_STRING:
        lept_set_string(dst, src->u.s.s, src->u.s.len);
        break;
    case LEPT_ARRAY:
        //数组
        lept_set_array(dst, src->u.a.size);
        for (i = 0; i < src->u.a.size; i++) 
            lept_copy(&dst->u.a.e[i], &src->u.a.e[i]);
        dst->u.a.size = src->u.a.size;
        break;
    case LEPT_OBJECT:
        //对象
        lept_set_object(dst, src->u.a.size);
        for (i = 0; i < src->u.a.size; i++) {
            //k复制
            lept_value* val = lept_set_object_value(dst, src->u.o.m[i].k, src->u.o.m[i].klen);
            //v复制
            lept_copy(val, &src->u.o.m[i].v);
        }
        dst->u.o.size = src->u.o.size;
        break;
    default:
        lept_free(dst);
        memcpy(dst, src, sizeof(lept_value));
        break;
    } 
}

//移动语意
void lept_move(lept_value* dst, lept_value* src) {
    assert(dst != NULL && src != NULL && src != dst);
    lept_free(dst);
    memcpy(dst, src, sizeof(lept_value));
    lept_init(src);
}

//交换值接口
void lept_swap(lept_value* lhs, lept_value* rhs) {
    assert(lhs != NULL && rhs != NULL);
    if (lhs != rhs) {
        lept_value temp;
        memcpy(&temp, lhs, sizeof(lept_value));
        memcpy(lhs, rhs, sizeof(lept_value));
        memcpy(rhs, &temp, sizeof(lept_value));
    }
}


/*解析操作*/
//null、false、true
static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type) {
    size_t i;
    EXPECT(c, literal[0]);
    for (i = 0; literal[i + 1]; i++)
        if (c->json[i] != literal[i + 1])
            return LEPT_PARSE_INVALID_VALUE;
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}

// ws = *(%x20 / %x09 / %x0A / %x0D) c->json定位到value的头个字符
static void lept_parse_whitespace(lept_context* c) {
    const char* p = c->json;
    //如果*p连着是这几种类型，移动指针
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')// \t制表 \n换行 \r回车
        p++;
    //把移动后的指针赋给初始值
    c->json = p;
}

/* null  = "null" */
static int lept_parse_null(lept_context* c, lept_value* v) {
    //判断c指向的第一个位置是否是'n',并且移动指针
    EXPECT(c, 'n');

    //此时字符串为"ull"，如果这些位置不是相应的字符，返回解析非法数据
    if (c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l')
        return LEPT_PARSE_INVALID_VALUE;

    //指针后移3个偏移量
    c->json += 3;
    v->type = LEPT_NULL;
    return LEPT_PARSE_OK;
}

/* false  = "false" */
static int lept_parse_false(lept_context* c, lept_value* v) {
    //判断c指向的第一个位置是否是'f',并且移动指针
    EXPECT(c, 'f');

    //此时字符串为"alse"，如果这些位置不是相应的字符，返回解析非法数据
    if (c->json[0] != 'a' || c->json[1] != 'l' || c->json[2] != 's' || c->json[3] != 'e')
        return LEPT_PARSE_INVALID_VALUE;

    //指针后移4个偏移量
    c->json += 4;
    v->type = LEPT_FALSE;
    return LEPT_PARSE_OK;
}

/* true  = "true" */
static int lept_parse_true(lept_context* c, lept_value* v) {
    //判断c指向的第一个位置是否是't',并且移动指针
    EXPECT(c, 't');

    //此时字符串为"ull"，如果这些位置不是相应的字符，返回解析非法数据
    if (c->json[0] != 'r' || c->json[1] != 'u' || c->json[2] != 'e')
        return LEPT_PARSE_INVALID_VALUE;

    //指针后移3个偏移量
    c->json += 3;
    v->type = LEPT_TRUE;
    return LEPT_PARSE_OK;
}

//解析数字
static int lept_parse_number(lept_context* c, lept_value* v) {
    //char* end;
    ///* \TODO validate number */
    //v->n = strtod(c->json, &end);
    //if (c->json == end || c->json[0] == '+' || c->json[0] == '.' || *(end - 1) == '.' || c->json[0] == 'i' || c->json[0] == 'I' || c->json[0] == 'N' || c->json[0] == 'n')
    //    return LEPT_PARSE_INVALID_VALUE;
    //c->json = end;
    //v->type = LEPT_NUMBER;
    ////printf("%lf\n", v->n);
    //return LEPT_PARSE_OK;

    const char* p = c->json;
    //负号正号处理
    if (*p == '-') p++;
    //if (*p == '+') return LEPT_PARSE_INVALID_VALUE;
    if (*p == '0') p++;
    else {
        if(!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    if (*p == '.') {
        p++;
        if(!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    if (*p == 'e' || *p == 'E'){
        p++;
        if (*p == '+' || *p == '-') p++;
        if(!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
        return LEPT_PARSE_NUMBER_TOO_BIG;
    c->json = p;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

//数据进栈, 返回数据存入的起始指针
static void* lept_context_push(lept_context* c, size_t size) {
    void* ret;
    assert(size > 0);
    if (c->top + size >= c->size) {
        if (c->size == 0)
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        while (c->top + size >= c->size)
            c->size += c->size >> 1;  /* c->size * 1.5 */
        c->stack = (char*)realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

//数据出栈
static void* lept_context_pop(lept_context* c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

//解析16位数字
static const char* lept_parse_hex4(const char* p, unsigned* u) {
    int i;
    *u = 0;
    for (i = 0; i < 4; i++) {
        char ch = *p++;
        *u <<= 4;
        if (ch >= '0' && ch <= '9') *u |= ch - '0';
        else if (ch >= 'A' && ch <= 'F') *u |= ch - ('A' - 10);
        else if (ch >= 'a' && ch <= 'f') *u |= ch - ('a' - 10);
        else return NULL;
    }
    return p;
}

//码点编码为UTF-8
static void lept_encode_utf8(lept_context* c, unsigned u) {
    if (u <= 0x7F)
        PUTC(c, u & 0xFF);
    else if (u <= 0x7FF) {
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
        PUTC(c, 0x80 | (u & 0x3F));
    }
    else if (u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >> 6) & 0x3F));
        PUTC(c, 0x80 | (u & 0x3F));
    }
    else {
        assert(u <= 0x10FFFF);
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >> 6) & 0x3F));
        PUTC(c, 0x80 | (u & 0x3F));
    }
}

#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)

/* 解析 JSON 字符串，把结果写入 str 和 len */
/* str 指向 c->stack 中的元素，需要在 c->stack  */
//解析字符串
static int lept_parse_string_raw(lept_context* c, char** str, size_t* len) {
    unsigned u, u2;
    size_t head = c->top;//head备份栈顶, len记录栈顶移动值
    const char* p;
    EXPECT(c, '\"');//判断c是否为string类型
    p = c->json;//p指向的是字符串开始的字符

    for (;;) {
        char ch = *p++;
        switch (ch) {
        case '\\': {
            switch (*p++) {
            case '\"': PUTC(c, '\"'); break;/* 显示" */
            case '\\': PUTC(c, '\\'); break;/* 显示\ */
            case '/':  PUTC(c, '/'); break; /* 显示/ */
            case 'b':  PUTC(c, '\b'); break;/* 转义  */
            case 'f':  PUTC(c, '\f'); break;/* 转义  */
            case 'n':  PUTC(c, '\n'); break;/* 转义  */
            case 'r':  PUTC(c, '\r'); break;/* 转义  */
            case 't':  PUTC(c, '\t'); break;/* 转义  */
            case 'u':
                if (!(p = lept_parse_hex4(p, &u)))
                    STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                else if (u >= 0xD800 && u <= 0xDBFF) {//对代理对的处理
                    if (*p++ != '\\')
                        STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                    if (*p++ != 'u')
                        STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                    if (!(p = lept_parse_hex4(p, &u2)))
                        STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                    if (u2 < 0xDC00 || u2>0xDFFF)
                        STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                    u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                }
                lept_encode_utf8(c, u);
                break;
            default:
                c->top = head;
                return LEPT_PARSE_INVALID_STRING_ESCAPE;
            }
            break;
        }

        case '\"':
            *len = c->top - head;
            c->json = p;
            *str = lept_context_pop(c, *len);
            return LEPT_PARSE_OK;
        case '\0':
            c->top = head;
            return LEPT_PARSE_MISS_QUOTATION_MARK;
        default:
            if ((unsigned char)ch < 0x20) {  //剩余的情况下有不合法字符串的情况：%x00 至 %x1F
                c->top = head;
                return LEPT_PARSE_INVALID_STRING_CHAR;//不合法的字符串
            }
            PUTC(c, ch); //把解析的字符串压栈
        }
    }
}

//写入 `lept_value`
static int lept_parse_string(lept_context* c, lept_value* v) {
    int ret;
    char* s;
    size_t len;
    if ((ret = lept_parse_string_raw(c, &s, &len)) == LEPT_PARSE_OK)
        lept_set_string(v, s, len);
    return ret;
}

static int lept_parse_value(lept_context* c, lept_value* v); /* 前向声明 */

//解析数组
static int lept_parse_array(lept_context* c, lept_value* v) {
    size_t i, size = 0;
    int ret;//记录错误类型
    EXPECT(c, '[');
    lept_parse_whitespace(c);
    if (*c->json == ']') {
        c->json++;
        lept_set_array(v, 0);
        return LEPT_PARSE_OK;
    }
    for (;;) {
        lept_value e;//生存临时的lept_value,用于存储数组内的不同元素
        lept_init(&e);
        if ((ret = lept_parse_value(c, &e)) != LEPT_PARSE_OK)//重新对c->json内的字符串进行解析，存入e中
            break;
        memcpy(lept_context_push(c, sizeof(lept_value)), &e, sizeof(lept_value));//把e中解析好的JSON值放入c的栈中
        size++;
        lept_parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            lept_parse_whitespace(c);
        }
        else if (*c->json == ']') {
            c->json++;
            v->type = LEPT_ARRAY;
            v->u.a.size = size;
            size *= sizeof(lept_value);
            memcpy(v->u.a.e = (lept_value*)malloc(size), lept_context_pop(c, size), size);//把c的栈内元素弹出，放入到v节点下的另一个数组
            return LEPT_PARSE_OK;
        }
        else {
            ret = LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        }
    }
    /* Pop and free values on the stack */
    for (i = 0; i < size; i++)
        lept_free((lept_value*)lept_context_pop(c, sizeof(lept_value)));
    return ret;
}

//解析对象
static int lept_parse_object(lept_context* c, lept_value* v) {
    size_t i, size;
    lept_member m;
    int ret;
    EXPECT(c, '{');
    lept_parse_whitespace(c);
    if (*c->json == '}') {
        c->json++;
        lept_set_object(v, 0);
        return LEPT_PARSE_OK;
    }
    m.k = NULL;
    size = 0;
    for (;;) {
        char* str;
        lept_init(&m.v);
        /* 1. parse key */
        if (*c->json != '"') {
            ret = LEPT_PARSE_MISS_KEY;
            break;
        }
        if ((ret = lept_parse_string_raw(c, &str, &m.klen)) != LEPT_PARSE_OK)
            break;
        memcpy(m.k = (char*)malloc(m.klen + 1), str, m.klen);
        m.k[m.klen] = '\0';
        /* 2. parse ws colon ws */
        lept_parse_whitespace(c);
        if (*c->json != ':') {
            ret = LEPT_PARSE_MISS_COLON;
            break;
        }
        c->json++;
        lept_parse_whitespace(c);
        /* 3. parse value */
        if ((ret = lept_parse_value(c, &m.v)) != LEPT_PARSE_OK)
            break;
        memcpy(lept_context_push(c, sizeof(lept_member)), &m, sizeof(lept_member));
        size++;
        m.k = NULL; 
        /* ownership is transferred to member on stack */
        /* 4. parse ws [comma | right-curly-brace] ws */
        lept_parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            lept_parse_whitespace(c);
        }
        else if (*c->json == '}') {
            size_t s = sizeof(lept_member) * size;
            c->json++;
            v->type = LEPT_OBJECT;
            v->u.o.size = size;
            memcpy(v->u.o.m = (lept_member*)malloc(s), lept_context_pop(c, s), s);
            return LEPT_PARSE_OK;
        }
        else {
            ret = LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
    }
    /* 5. Pop and free members on the stack */
    free(m.k);
    for (i = 0; i < size; i++) {
        lept_member* m = (lept_member*)lept_context_pop(c, sizeof(lept_member));
        free(m->k);
        lept_free(&m->v);
    }
    v->type = LEPT_NULL;
    return ret;
}

/* value = null / false / true / number / string / array */
static int lept_parse_value(lept_context* c, lept_value* v) {//返回json中的value类型
    switch (*c->json) {
    /*case 'n':  return lept_parse_null(c, v);
    case 'f':  return lept_parse_false(c, v);
    case 't':  return lept_parse_true(c, v);*/
    case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
    case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
    case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
    default:   return lept_parse_number(c, v);
    case '{':  return lept_parse_object(c, v);//object
    case '[':  return lept_parse_array(c, v);//array
    case '"':  return lept_parse_string(c, v);//string
    case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
}

/*解析JSON*/
int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;//记录解析后的返回枚举值
    assert(v != NULL);

    //创建c并初始化stack
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    lept_init(v);
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
            //处理最后的ws的时候，v->type会被赋值为lept_parse_value的返回值
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
            v->type = LEPT_NULL;
        }
    }

    assert(c.top == 0);//释放前加入断言确保所有数据都被弹出
    free(c.stack);
    return ret;
}


/*生成操作*/
//生成字符串 v->u.s.s的字符串输入到c中
#if 0
static void lept_stringify_string(lept_context* c, const char* s, size_t len) {
    size_t i;
    assert(s != null);
    putc(c, '"');
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        switch (ch) {
        case '\"': puts(c, "\\\"", 2); break;
        case '\\': puts(c, "\\\\", 2); break;
        case '\b': puts(c, "\\b", 2); break;
        case '\f': puts(c, "\\f", 2); break;
        case '\n': puts(c, "\\n", 2); break;
        case '\r': puts(c, "\\r", 2); break;
        case '\t': puts(c, "\\t", 2); break;
        default:
            if (ch < 0x20) {
                char buffer[7];
                sprintf_s(buffer, 7, "\\u%04x", ch);
                puts(c, buffer, 6);
            }
            else
                putc(c, s[i]);
        }
    }
    putc(c, '"');
}
#else
static void lept_stringify_string(lept_context* c, const char* s, size_t len) {
    static const char hex_digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    size_t i, size;
    char* head, * p;
    assert(s != NULL);
    p = head = lept_context_push(c, size = len * 6 + 2); /* "\u00xx..." */
    *p++ = '"';
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        switch (ch) {
        case '\"': *p++ = '\\'; *p++ = '\"'; break;
        case '\\': *p++ = '\\'; *p++ = '\\'; break;
        case '\b': *p++ = '\\'; *p++ = 'b';  break;
        case '\f': *p++ = '\\'; *p++ = 'f';  break;
        case '\n': *p++ = '\\'; *p++ = 'n';  break;
        case '\r': *p++ = '\\'; *p++ = 'r';  break;
        case '\t': *p++ = '\\'; *p++ = 't';  break;
        default:
            if (ch < 0x20) {
                *p++ = '\\'; *p++ = 'u'; *p++ = '0'; *p++ = '0';
                *p++ = hex_digits[ch >> 4];
                *p++ = hex_digits[ch & 15];
            }
            else
                *p++ = s[i];
        }
    }
    *p++ = '"';
    c->top -= size - (p - head);
}
#endif

static void lept_stringify_value(lept_context* c, const lept_value* v) {
    size_t i;
    switch (v->type) {
    case LEPT_NULL:   PUTS(c, "null", 4); break;
    case LEPT_FALSE:  PUTS(c, "false", 5); break;
    case LEPT_TRUE:   PUTS(c, "true", 4); break;
    case LEPT_NUMBER:
        c->top -= 32 - sprintf_s(lept_context_push(c, 32), 32, "%.17g", v->u.n);//"%.17g" 是足够把双精度浮点转换成可还原的文本
        break;
    case LEPT_STRING:
        lept_stringify_string(c, v->u.s.s, v->u.s.len);
        break;
    case LEPT_ARRAY:
        PUTC(c, '[');
        for (i = 0; i < v->u.a.size; i++) {
            if (i > 0)
                PUTC(c, ',');
            lept_stringify_value(c, &v->u.a.e[i]);
        }
        PUTC(c, ']');
        break;
    case LEPT_OBJECT:
        PUTC(c, '{');
        for (i = 0; i < v->u.o.size; i++) {
            if (i > 0)
                PUTC(c, ',');
            lept_stringify_string(c, v->u.o.m[i].k, v->u.o.m[i].klen);
            PUTC(c, ':');
            lept_stringify_value(c, &v->u.o.m[i].v);
        }
        PUTC(c, '}');
        break;
    default: assert(0 && "invalid type");
    }
}

//字符串化
char* lept_stringify(const lept_value* v, size_t* length) {
    lept_context c;
    assert(v != NULL);
    c.stack = (char*)malloc(c.size = LEPT_PARSE_STRINGIFY_INIT_SIZE);
    c.top = 0;
    lept_stringify_value(&c, v);
    if (length)
        *length = c.top;//根据c.top对传进来的*length赋值
    PUTC(&c, '\0');
    return c.stack;
}