#if not defined DEBUG_H
#define DEBUG_H


#if defined D1
    #define P1(...) printf(__VA_ARGS__)
#else
    #define P1(...)
#endif


#if defined D2
    #define P2(...) printf(__VA_ARGS__)
#else
    #define P2(...)
#endif


#if defined D3
    #define P3(...) printf(__VA_ARGS__)
#else
    #define P3(...)
#endif


#endif
