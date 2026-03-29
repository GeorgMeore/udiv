#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>

/*
 * This code is based on the wonderful series of articles on the topic
 * made by the author of libdivide, if you are interested you should check it out
 *
 * https://ridiculousfish.com/blog/posts/labor-of-division-episode-i.html
 */

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

/* NOTE: we need this because compiler vendors are fucking morons
 * and use UB as an excuse for breaking your code */
static u64 rsh(u64 x, u8 n)
{
	u8 q = n / 64, r = n % 64;
	return !q * (x >> r);
}

/*
 * If we want to compute (2^p / x), where (x < 2^p) and x is not a power of 2,
 * we can do that using just p-bit unsigned subtract and divide.
 *
 * Let's start with long (shift-subtract) division:
 *
 *   1000..0000 <- 2^p (p + 1 bits)
 *   ----------
 *       1..1.. <- x (n bits)
 *   1..1..0000 <- x * 2^(p + 1 - n)
 *    1..1..000 <- x * 2^(p - n)
 *
 * It's obvious that (x * 2^(p + 1 - n) > 2^p) and (x * 2^(p - n) < 2^p), so
 * the first subtract must happen with the latter value.
 * Doing the subtract is trivial since (2^p - v) mod 2^p = (0 - v) mod 2^p,
 * or just (-v mod 2^p). After that we're left with the task of dividing
 * a p-bit remainder by an n-bit value.
 *
 * So the following algorithm will compute the correct quotient
 * and remainder in q and r respectively:
 *
 *   q' = 2^(p - n)
 *   r' = -(x * q') mod 2^p
 *   q  = r' / x + q'
 *   r  = r' % x
 */

/* ceil(2^64 / v) */
u64 inv32(u32 v)
{
	u8 n = nlz(v);
	u64 r = -((u64)v << n);
	return ((u64)1 << n) + (r / v) + !!(r % v);
}

/* floor(x * inv / 2^64) */
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
	u128 xn = {x.lo << n, (x.hi << n) | rsh(x.lo, 64 - n)};
	u64 q1, r1;
	D(xn.hi, xn.lo >> 32, yn, &q1, &r1);
	u64 q2, r2;
	D(r1, xn.lo & 0xFFFFFFFF, yn, &q2, &r2);
	*r = r2 >> n;
	q->lo = q1 << 32 | q2;
	q->hi = q0;
}

/* ceil(2^128 / x) */
u128 inv64(u64 x)
{
	u8 n = nlz(x);
	u128 t = {-(x << n) , ~(u64)0};
	u128 q;
	u64 r;
	divu128u64(t, x, &q, &r);
	u64 d = ((u64)1 << n) + !!r;
	q.lo += d;
	q.hi += (q.lo < d);
	return q;
}

static u128 mulu64(u64 x, u64 y)
{
	u64 x0 = x & 0xFFFFFFFF, x1 = x >> 32;
	u64 y0 = y & 0xFFFFFFFF, y1 = y >> 32;
	u64 t = x0*y1 + y0*x1;
	u64 of1 = t < x0*y1;
	u64 lo = x0*y0 + (t << 32);
	u64 of2 = lo < x0*y0;
	u64 hi = x1*y1 + (t >> 32) + of2 + (of1 << 32);
	return (u128){lo, hi};
}

/* floor(x * inv / 2^128) */
u64 div64inv(u64 x, u128 inv)
{
	u128 lo = mulu64(x, inv.lo);
	u128 hi = mulu64(x, inv.hi);
	return hi.hi + (lo.hi + hi.lo < lo.hi);
}

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
	u64 c = div32inv(a, i);
	assert(c == 2223);
	for (u32 r = 0; r < 10000; r++) {
		u32 a = rand();
		if (a & (a - 1)) {
			u64 i = inv32(a);
			for (u32 j = 0; j < 1000; j++) {
				u32 b = rand();
				assert(b / a == div32inv(b, i));
			}
		}
	}
}

void testinv64(void)
{
	u64 c1 = div64inv(15974531580214495800UL, inv64(2817953302490816533UL));
	assert(c1 == 5);
	u64 c2 = div64inv(3086287087048080399UL, inv64(28175330249086533UL));
	assert(c2 == 109);
	for (u32 r = 0; r < 10000; r++) {
		u64 a = (u64)rand() * (u64)rand();
		if (a & (a - 1)) {
			u128 i = inv64(a);
			for (u32 j = 0; j < 1000; j++) {
				u64 b = (u64)rand() * (u64)rand();
				assert(b / a == div64inv(b, i));
			}
		}
	}
}

/* TODO: handle powers of two in inv32 and inv64 */
int main(void)
{
	srand(time(0));
	testinv32();
	testinv64();
	testdivu128u64();
	return 0;
}
