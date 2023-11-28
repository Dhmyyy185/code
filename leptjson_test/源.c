#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <errno.h>   /* errno, ERANGE */
#include <math.h>    /* HUGE_VAL */
#include <ctype.h> /*ISDIGIT*/

typedef struct {
    const char* json;
    char* stack;
    size_t size, top;
}lept_context;

int back(int i) {
	printf("%d\n", i);
}

#define PUTC(c, ch) do { *(char*)lept_context_push(c, sizeof(char)) = (ch); } while(0)

static void* lept_context_push(lept_context* c, size_t size) {
    void* ret;
    //assert(size > 0);
    if (c->top + size >= c->size) {
        if (c->size == 0)
            c->size = 256;
        while (c->top + size >= c->size)
            c->size += c->size >> 1;  /* c->size * 1.5 */
        c->stack = (char*)realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    printf("%c\n", *(char*)ret);
    return ret;
}

static void* lept_context_pop(lept_context* c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

//int main()
//{
//	lept_context c1;
//	c1.json = "Nan";
//	lept_context* c = &c1;
//	const char* p = c->json;
//	//char* ptr;
//	double ret;
//	if (*p == '-') p++;
//	//if (*p == '+') return LEPT_PARSE_INVALID_VALUE;
//	if (*p == '0') p++;
//	else {
//		if (!isdigit(*p)) {
//			printf("JSON数字不符合\n");
//			return 0;
//		}
//		for (p++; isdigit(*p); p++);
//	}
//	if (*p == '.') {
//		p++;
//		if (!isdigit(*p)) {
//			printf("JSON数字不符合\n");
//			return 0;
//		}
//		for (p++; isdigit(*p); p++);
//	}
//	if (*p == 'e' || *p == 'E') {
//		p++;
//		if (*p == '+' || *p == '-') p++;
//		if (!isdigit(*p)) {
//			printf("JSON数字不符合\n");
//			return 0;
//		}
//		for (p++; isdigit(*p); p++);
//	}
//	errno = 0;
//	ret = strtod(c->json, NULL);
//	if (errno == ERANGE && ret == HUGE_VAL) printf("JSON数字值过大\n");
//	c->json = p;
//	printf("c->json指向:%c\n", *c->json);
//	printf("数字（double）是 %lf", ret);
//
//	int i = 5;
//
//	
//
//	/*switch (i) {
//	case 1:  back(1);
//	case 2:  back(2);
//	case 3:  back(3);
//	case 5: back(5);
//	default:   back(i);
//	}*/
//
//
//
//	return(0);
//}

//int main(void)
//{
//    const char* p = "111.11 -2.22 Nan nan(2) inF 0X1.BC70A3D70A3D7P+6  1.18973e+4932zzz";
//    printf("Parsing '%s':\n", p);
//    char* end;
//    double f;
//    for (f = strtod(p, &end); p != end; f = strtod(p, &end))
//    {
//        printf("'%.*s' -> ", (int)(end - p), p);//I can't understand this line
//        p = end;
//        if (errno == ERANGE) {
//            printf("range error, got ");
//            errno = 0;
//        }
//        printf("%f\n", f);
//    }
//}

//int main() {
//    /*lept_context c1;
//    
//    lept_context* c = &c1;
//
//    c->json = "123456";
//    c->stack = NULL;
//    c->size = c->top = 0;
//
//    const char* p = c->json;
//    c->size = 256;
//    c->stack = (char*)realloc(c->stack, c->size);
//    for (int i = 0; i < 5; i++) {
//        char ch = *p++;
//        PUTC(c, ch);
//    }
//
//    printf("%.5s", c->stack);*/
//
//    const char* c = "Hello";
//    
//    size_t len = 5;
//
//    char* s = NULL;
//    s = (char*)malloc(len + 1);
//
//    //memcpy(s, c, len);
//    //s[len] = '\0';
//
//    printf("%d", sizeof(s));
//}

int main() {

    char* c = "\"Hello\\nWorld\"";
    int n = 0, flag = 0;

    char* p = (char*)malloc(20);
    char* ch = c;
    switch (*ch++) {
        if (flag == 2) break;
    case '\"': {
        p[n++] = '\"';
        flag++;
        break;
    }
    case '\\': {
        switch (*ch++) {
        case '\"': p[n++] = '\"'; break;/* 显示" */
        case '\\': p[n++] = '\\'; break;/* 显示\ */
        case '\/':  p[n++] = '\/'; break; /* 显示/ */
        case 'b':  p[n++] = '\b'; break;/* 转义  */
        case 'f':  p[n++] = '\f'; break;/* 转义  */
        case 'n':  p[n++] = '\n'; break;/* 转义  */
        case 'r':  p[n++] = '\r'; break;/* 转义  */
        case 't':  p[n++] = '\t'; break;/* 转义  */
        default: break;
        }
    default: p[n++] = *ch;
    }
    }

    for (size_t i = 0; i < strlen(p); i++) {
        //if (c[i] == '\\') n++;
        printf("%c", c[i]);
    }
    //printf("%d\n", n);
    //printf("%d\n", strlen(p));


    return 0;
}