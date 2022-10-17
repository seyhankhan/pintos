#ifndef FIXED_POINT
#define FIXED_POINT

#define INTEGER_TO_FP(n, f) (n * x)
#define FP_TO_INTEGER(x, f) (x / f)
#define FP_ROUNDED(x, f) ((x >= 0) ? ((x + (f / 2)) / f) : ((x - (f / 2)) / f))
#define FP_ADD_FP(x, y) (x + y)
#define FP_SUBTRACT_FP(x, y) (x - y)
#define FP_ADD_INT(x, n, f) (x + (n * f))
#define FP_SUBTRACT_INT(x, n, f) (x - (n * f))
#define FP_MULT_FP(x, y, f) (((int64_t) x) * y / f)
#define FP_MULT_INT(x, n) (x * n)
#define FP_DIV_FP(x, y, f ) (((int64_t) x) * f / y)
#define FP_DIV_INT(x, n) (x / n)

#endif
