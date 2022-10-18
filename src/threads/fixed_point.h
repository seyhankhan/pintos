#ifndef FIXED_POINT
#define FIXED_POINT


// F = 2^q - use left shift
// q = 14, p = 17
#define Q 14
#define F (1 << Q)

#define INTEGER_TO_FP(n) (n * F)
#define FP_TO_INTEGER(x) (x / F)
#define FP_ROUNDED(x) ((x >= 0) ? ((x + (F / 2)) / F) : ((x - (F / 2)) / F))
#define FP_ADD_FP(x, y) (x + y)
#define FP_SUBTRACT_FP(x, y) (x - y)
#define FP_ADD_INT(x, n) (x + (n * F))
#define FP_SUBTRACT_INT(x, n) (x - (n * F))
#define FP_MULT_FP(x, y) (((int64_t) x) * y / F)
#define FP_MULT_INT(x, n) (x * n)
#define FP_DIV_FP(x, y) (((int64_t) x) * F / y)
#define FP_DIV_INT(x, n) (x / n)

#endif
