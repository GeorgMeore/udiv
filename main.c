#include <stdio.h>
#include <assert.h>

typedef signed char   i8;
typedef unsigned char u8;
typedef signed int    i32;
typedef unsigned int  u32;
typedef signed long   i64;
typedef unsigned long u64;

u8 nlz(u64 x)
{
	u8 n = 64;
	u8 c; /* binary search condition */
	c = !!(x >> 32), x >>= c << 5, n -= c << 5;
	c = !!(x >> 16), x >>= c << 4, n -= c << 4;
	c = !!(x >> 8),  x >>= c << 3, n -= c << 3;
	c = !!(x >> 4),  x >>= c << 2, n -= c << 2;
	c = !!(x >> 2),  x >>= c << 1, n -= c << 1;
	c = !!(x >> 1),  x >>= c, n -= !!c;
	return (n - x);
}

/* NOTE: this computes ceil(2^64 / v) via one step of long division + hardware divide */
u64 inverse(u32 v)
{
	assert(v & (v - 1));
	u8 n = nlz(v); /* smallest n such that v*2^n < 2^64 */
	u64 r = 0 - ((u64)v << n); /* 2^64 - v*2^n */
	u64 q = (u64)1 << n; /* 2^n goes into the quitient */
	return q + (r / v) + !!(r % v);
}

u32 div(u32 x, u64 inv)
{
	/* compute floor(x * inv / 2^64) */
	u64 lo = inv & 0xFFFFFFFF, hi = inv >> 32;
	return (hi*x + (lo * x >> 32)) >> 32;
}

int main(void)
{
	u32 a = 8638219;
	u32 b = 3885;
	u64 i = inverse(b);
	printf("%lu(%lx)\n", i, i);
	printf("%u / %u = %u\n", a, b, div(a, i));
	return 0;
}
