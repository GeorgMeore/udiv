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
	u8 c;
	c = !!(x >> 32) << 5;
	x >>= c, n -= c;
	c = !!(x >> 16) << 4;
	x >>= c, n -= c;
	c = !!(x >> 8) << 3;
	x >>= c, n -= c;
	c = !!(x >> 4) << 2;
	x >>= c, n -= c;
	c = !!(x >> 2) << 1;
	x >>= c, n -= c;
	c = x >> 1;
	x >>= c, n -= c;
	return (n - x);
}

/* NOTE: computes ceil(2^64 / v) via one step of long division
 * plus hardware divide, v must not be a power of 2 */
u64 inv32(u32 v)
{
	u8 n = nlz(v);          /* smallest n such that v*2^n < 2^64 */
	u64 r = -((u64)v << n); /* 2^64 - v*2^n */
	u64 q = (u64)1 << n;    /* 2^n goes into the quotient */
	return q + (r / v) + !!(r % v);
}

/* NOTE: compute floor(x * inv / 2^64) */
u32 div32inv(u32 x, u64 inv)
{
	u64 lo = inv & 0xFFFFFFFF, hi = inv >> 32;
	return (hi*x + (lo * x >> 32)) >> 32;
}

typedef struct { u64 lo, hi; } u128;

/*
 * We want to divide a 3-digit number [x2 x1 x0] by 2-digit number [y1 y0]
 * (each digit is a 32-bit value). We assume that 2^31 <= y1 and x2 < y1,
 * the caller is responsible for ensuring that.
 *
 * We approximate the quotient by dividing [x2 x1] by [y1]:
 *   [x2 x1] = qhat * [y1] + rhat.
 * Notice that both qhat and rhat must fit into 32 bits.
 * Let's find out how wrong this qhat is:
 *
 *   [x2 x1 x0] - qhat * [y1 y0]
 *     = [x2 x1 0] + x0 - qhat * [y1 0] - qhat * y0
 *     = ([x2 x1 0] - qhat * [y1 0]) + x0 - qhat * y0
 *     = [rhat 0] + x0 - qhat * y0 = [rhat x0] - qhat * y0
 *
 * We can also show that qhat can never be too small:
 *
 *   floor([x2 x1 x0] / [y1 y0]) <= floor([x2 x1 x0] / [y1 0])
 *   floor([x2 x1 x0] / [y1 0])
 *     = floor([x2 x1 0] / [y1 0] + [x0] / [y1 0])
 *     = floor([qhat] + [rhat x0] / [y1 0])
 *         Here we notice that rhat < y1, therefore [rhat x0] / [y1 0] < 1
 *     = qhat
 *
 * So qhat can only be larger than the real floor quotient.
 */
static void D(u64 x21, u32 x0, u64 y, u64 *q, u64 *r)
{
	u32 y1 = y >> 32, y0 = y & 0xFFFFFFFF;
	u64 qhat = x21 / y1; /* 2^31 <= y1 < 2^32, so qhat fits in 32 bits */
	u64 rhat = x21 % y1;
	u64 v1 = qhat * y0;
	u64 v2 = (rhat << 32) | x0;
	if (v1 > v2)
		qhat -= 1 + (v1 - v2 > y); /* v1 - v2 <= 2*y */
	*q = qhat;
	u64 x10 = ((x21 & 0xFFFFFFFF) << 32) | x0;
	*r = x10 - qhat*y; /* we don't care for higher bits */
}

void divu128u64(u128 x, u64 y, u128 *q, u64 *r)
{
	u64 q0 = x.hi / y;
	x.hi = x.hi % y; /* now shifting x by nlz(y) is safe */
	u8 n = nlz(y);
	u64 yn = y << n;
	u128 xn = {x.lo << n, (x.hi << n) | (x.lo >> (64 -n))};
	u64 q1, r1;
	D(xn.hi, xn.lo >> 32, yn, &q1, &r1);
	u64 q2, r2;
	D(r1, xn.lo & 0xFFFFFFFF, yn, &q2, &r2);
	*r = r2 >> n;
	q->lo = q1 << 32 | q2;
	q->hi = q0;
}

/* TODO: u128 inv64(u64 x) */
/* TODO: u64 div64inv(u64 x, u128 inv) */

void testdivu128u64(void)
{
	u128 x = {0x706cec5e0be2831a, 0x9485acc9b0c7b807}; /* 197419823412375981738123985713249813274 */
	u128 q;
	u64 r;
	u64 y = 1836821932753;
	divu128u64(x, y, &q, &r);
	assert(r == 489136138301);
	assert(q.hi == 5826449);
	assert(q.lo == 7617459773914094157);
}

void testinv32(void)
{
	u32 a = 8638219;
	u32 b = 3885;
	u64 i = inv32(b);
	printf("%lu(%lx)\n", i, i);
	u64 c = div32inv(a, i);
	assert(c == 2223);
}

/* TODO: randomized testers */
int main(void)
{
	testinv32();
	testdivu128u64();
	return 0;
}
