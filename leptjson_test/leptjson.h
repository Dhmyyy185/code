#ifndef LEPTJSON_H__//������ֹ�������ͷ�ļ�
#define LEPTJSON_H__

#include <stddef.h>
//ö�ٿ��ܻ���ֵ�7����������
typedef enum { LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT } lept_type;

#define LEPT_KEY_NOT_EXIST ((size_t)-1)

typedef struct lept_value lept_value;//ǰ������
typedef struct lept_member lept_member;

//ֻ��Ҫʵ��null��true��false�Ľ���������ֻ��Ҫ�洢һ��lept_typeö������
struct lept_value {
    union {
        struct { lept_member* m; size_t size, capacity; }o;/* object: members, member count, capacity */
        struct { lept_value* e; size_t size, capacity; }a;/* array:  elements, element count(��ǰ��ЧԪ�ظ���), capacity(��ǰ����Ԫ�ظ���) */
        struct { char* s; size_t len; }s; /*string*/
        double n; /*number*/
    }u;
    lept_type type;
};

struct lept_member {
    char* k; size_t klen;   /* member key string, key string length */
    lept_value v;           /* member value */
};

//����ֵΪ����ö��ֵ
enum {
    LEPT_PARSE_OK = 0,//�޴��󷵻ظ�ֵ
    LEPT_PARSE_EXPECT_VALUE,//JSONֻ���пհ�
    LEPT_PARSE_INVALID_VALUE,//ֵ��������������ֵ
    LEPT_PARSE_ROOT_NOT_SINGULAR,//һ��ֵ���ڿհ�֮���������ַ�
    LEPT_PARSE_NUMBER_TOO_BIG,//����ֵ̫��
    LEPT_PARSE_MISS_QUOTATION_MARK,//���Ų�ȫ
    LEPT_PARSE_INVALID_STRING_ESCAPE,//���Ϸ���ת���ַ�
    LEPT_PARSE_INVALID_STRING_CHAR,//���Ϸ����ַ�
    LEPT_PARSE_INVALID_UNICODE_HEX,//���Ϸ���16������
    LEPT_PARSE_INVALID_UNICODE_SURROGATE,//���Ϸ��Ĵ����
    LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET,//���Ż������Ų��Ϸ�
    LEPT_PARSE_MISS_KEY,
    LEPT_PARSE_MISS_COLON,
    LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET,
    LEPT_STRINGIFY_OK
};

#define lept_init(v) do { (v)->type = LEPT_NULL; } while(0)//�������з��ʺ���ǰ���ȳ�ʼ��������

int lept_parse(lept_value* v, const char* json);//����JSON
char* lept_stringify(const lept_value* v, size_t* length);//�����νṹת��ΪJSON�ı�

void lept_free(lept_value* v);

lept_type lept_get_type(const lept_value* v);//���ʽ���ĺ���,���ǻ�ȡ������

#define lept_set_null(v) lept_free(v)

int lept_get_boolean(const lept_value* v);//��ȡJSON(false��true)��ֵ
void lept_set_boolean(lept_value* v, int b);//д��JSON����ֵ

double lept_get_number(const lept_value* v);//��ȡJSON���ֵ���ֵ
void lept_set_number(lept_value* v, double n);//д��JSON����

const char* lept_get_string(const lept_value* v);//��ȡJSON�ַ���
size_t lept_get_string_length(const lept_value* v);//��ȡJSON�ַ����ĳ���
void lept_set_string(lept_value* v, const char* s, size_t len);//д��JSON�ַ���

void lept_set_array(lept_value* v, size_t capacity);//�ṩ��ʼ��������
size_t lept_get_array_size(const lept_value* v);//��ȡ���鳤��
size_t lept_get_array_capacity(const lept_value* v);//��ȡ��ǰ����
void lept_reserve_array(lept_value* v, size_t capacity);//��������
void lept_shrink_array(lept_value* v);//������С
void lept_clear_array(lept_value* v);//�������Ԫ��(��������)
lept_value* lept_get_array_element(const lept_value* v, size_t index);//��ȡ����Ԫ��
lept_value* lept_pushback_array_element(lept_value* v);//����ĩ��ѹ��һ��Ԫ�أ������µ�Ԫ��ָ��
void lept_popback_array_element(lept_value* v);//��������
lept_value* lept_insert_array_element(lept_value* v, size_t index);//��indexλ�ò���һ��Ԫ��
void lept_erase_array_element(lept_value* v, size_t index, size_t count);//ɾȥ��indexλ�ÿ�ʼ��count��Ԫ��(��������)

//member = string ws % x3A ws value
//object = % x7B ws[member * (ws % x2C ws member)] ws % x7D
void lept_set_object(lept_value* v, size_t capacity);//�ṩ��ʼ��������
size_t lept_get_object_size(const lept_value* v);//��ȡ�������󳤶�
size_t lept_get_object_capacity(const lept_value* v);//��ȡ��ǰ����
void lept_reserve_object(lept_value* v, size_t capacity);//��������
void lept_shrink_object(lept_value* v);//������С
void lept_clear_object(lept_value* v);//�������Ԫ��(��������)
const char* lept_get_object_key(const lept_value* v, size_t index);//��ȡ���������ֵ
size_t lept_get_object_key_length(const lept_value* v, size_t index);//��ȡ���������ֵ����
lept_value* lept_get_object_value(const lept_value* v, size_t index);//��ȡ���������ֵ���ֵ
size_t lept_find_object_index(const lept_value* v, const char* key, size_t klen);//�����Բ���JSON������ĳ�����Ƿ����
lept_value* lept_find_object_value(const lept_value* v, const char* key, size_t klen);//��ȡ����Ӧ��ֵ
lept_value* lept_set_object_value(lept_value* v, const char* key, size_t klen);/* ����������ֵ�Ե�ָ�� */
void lept_remove_object_value(lept_value* v, size_t index);

int lept_is_equal(const lept_value* lhs, const lept_value* rhs);//��ȱȽ�
void lept_copy(lept_value* dst, const lept_value* src);//��ȸ���
void lept_move(lept_value* dst, lept_value* src);//�ƶ�����
void lept_swap(lept_value* lhs, lept_value* rhs);//����ֵ�ӿ�

#endif /* LEPTJSON_H__ */