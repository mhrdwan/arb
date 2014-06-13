/*=============================================================================

    This file is part of ARB.

    ARB is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    ARB is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with ARB; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

=============================================================================*/
/******************************************************************************

    Copyright (C) 2012, 2013 Fredrik Johansson

******************************************************************************/

#include "zeta.h"
#include "hypgeom.h"

typedef struct
{
    arb_t P;
    arb_t Q;
    arb_t T;
    arb_t C;
    arb_t D;
    arb_t V;
} euler_bsplit_struct;

typedef euler_bsplit_struct euler_bsplit_t[1];

static void euler_bsplit_init(euler_bsplit_t s)
{
    arb_init(s->P);
    arb_init(s->Q);
    arb_init(s->T);
    arb_init(s->C);
    arb_init(s->D);
    arb_init(s->V);
}

static void euler_bsplit_clear(euler_bsplit_t s)
{
    arb_clear(s->P);
    arb_clear(s->Q);
    arb_clear(s->T);
    arb_clear(s->C);
    arb_clear(s->D);
    arb_clear(s->V);
}

static void
euler_bsplit_1_merge(euler_bsplit_t s, euler_bsplit_t L, euler_bsplit_t R,
                    long wp, int cont)
{
    arb_t t, u, v;

    arb_init(t);
    arb_init(u);
    arb_init(v);

    if (cont)
        arb_mul(s->P, L->P, R->P, wp);

    arb_mul(s->Q, L->Q, R->Q, wp);
    arb_mul(s->D, L->D, R->D, wp);

    /* T = LP RT + RQ LT*/
    arb_mul(t, L->P, R->T, wp);
    arb_mul(v, R->Q, L->T, wp);
    arb_add(s->T, t, v, wp);

    /* C = LC RD + RC LD */
    if (cont)
    {
        arb_mul(s->C, L->C, R->D, wp);
        arb_addmul(s->C, R->C, L->D, wp);
    }

    /* V = RD (RQ LV + LC LP RT) + LD LP RV */
    arb_mul(u, L->P, R->V, wp);
    arb_mul(u, u, L->D, wp);
    arb_mul(v, R->Q, L->V, wp);
    arb_addmul(v, t, L->C, wp);
    arb_mul(v, v, R->D, wp);
    arb_add(s->V, u, v, wp);

    arb_clear(t);
    arb_clear(u);
    arb_clear(v);
}

static void
euler_bsplit_1(euler_bsplit_t s, long n1, long n2, long N, long wp, int cont)
{
    if (n2 - n1 == 1)
    {
        arb_set_si(s->P, N); /* p = N^2 */
        arb_mul(s->P, s->P, s->P, wp);
        arb_set_si(s->Q, n1 + 1); /* q = (k + 1)^2 */
        arb_mul(s->Q, s->Q, s->Q, wp);
        arb_set_si(s->C, 1);
        arb_set_si(s->D, n1 + 1);
        arb_set(s->T, s->P);
        arb_set(s->V, s->P);
    }
    else
    {
        euler_bsplit_t L, R;
        long m = (n1 + n2) / 2;

        euler_bsplit_init(L);
        euler_bsplit_init(R);
        euler_bsplit_1(L, n1, m, N, wp, 1);
        euler_bsplit_1(R, m, n2, N, wp, 1);
        euler_bsplit_1_merge(s, L, R, wp, cont);
        euler_bsplit_clear(L);
        euler_bsplit_clear(R);
    }
}

static void
euler_bsplit_2(arb_t P, arb_t Q, arb_t T, long n1, long n2,
                        long N, long wp, int cont)
{
    if (n2 - n1 == 1)
    {
        if (n1 == 0)
        {
            arb_set_si(P, 1);
            arb_set_si(Q, 4 * N);
            arb_set_si(T, 1);
        }
        else
        {
            arb_si_pow_ui(P, 1 - 2*n1, 3, wp);
            arb_neg(P, P);

            arb_set_si(Q, 32 * n1);
            arb_mul_ui(Q, Q, N, wp);
            arb_mul_ui(Q, Q, N, wp);
        }

        arb_set(T, P);
    }
    else
    {
        arb_t P2, Q2, T2;
        long m = (n1 + n2) / 2;

        arb_init(P2);
        arb_init(Q2);
        arb_init(T2);

        euler_bsplit_2(P, Q, T, n1, m, N, wp, 1);
        euler_bsplit_2(P2, Q2, T2, m, n2, N, wp, 1);

        arb_mul(T, T, Q2, wp);
        arb_mul(T2, T2, P, wp);
        arb_add(T, T, T2, wp);

        if (cont)
            arb_mul(P, P, P2, wp);

        arb_mul(Q, Q, Q2, wp);

        arb_clear(P2);
        arb_clear(Q2);
        arb_clear(T2);
    }
}

static void
atanh_bsplit(arb_t s, ulong c, long a, long prec)
{
    arb_t t;
    hypgeom_t series;
    hypgeom_init(series);
    arb_init(t);

    fmpz_poly_set_ui(series->A, 1);
    fmpz_poly_set_coeff_ui(series->B, 0, 1);
    fmpz_poly_set_coeff_ui(series->B, 1, 2);
    fmpz_poly_set_ui(series->P, 1);
    fmpz_poly_set_ui(series->Q, c * c);

    arb_hypgeom_infsum(s, t, series, prec, prec);
    arb_mul_si(s, s, a, prec);
    arb_mul_ui(t, t, c, prec);
    arb_div(s, s, t, prec);

    arb_clear(t);
    hypgeom_clear(series);
}

static ulong
next_smooth(ulong n)
{
    ulong t, k;

    for (k = n; ; k++)
    {
        t = k;
        while (t % 2 == 0) t /= 2;
        while (t % 3 == 0) t /= 3;
        while (t % 5 == 0) t /= 5;
        if (t == 1)
            return k;
    }
}

static void
arb_log_ui_smooth(arb_t s, ulong n, long prec)
{
    ulong m, i, j, k;
    arb_t t;

    m = n;
    i = j = k = 0;
    while (m % 2 == 0) { m /= 2; i++; }
    while (m % 3 == 0) { m /= 3; j++; } 
    while (m % 5 == 0) { m /= 5; k++; }

    if (m != 1)
        abort();

    arb_init(t);

    prec += FLINT_CLOG2(prec);

    atanh_bsplit(s, 31, 14*i + 22*j + 32*k, prec);
    atanh_bsplit(t, 49, 10*i + 16*j + 24*k, prec);
    arb_add(s, s, t, prec);
    atanh_bsplit(t, 161, 6*i + 10*j + 14*k, prec);
    arb_add(s, s, t, prec);

    arb_clear(t);
}

void
arb_const_euler_eval(arb_t res, long prec)
{
    euler_bsplit_t sum;
    arb_t t, u, v, P2, T2, Q2;
    long bits, wp, wp2, n, N, M;

    bits = prec + 10;
    n = 0.086643397569993163677 * bits + 1;  /* log(2) / 8 */

    /* round n to have many trailing zeros, speeding up arithmetic,
       and make it smooth to allow computing the logarithm cheaply */
    if (n > 256)
    {
        int b = FLINT_BIT_COUNT(n);
        n = next_smooth((n >> (b-4)) + 1) << (b-4);
    }
    else
    {
        n = next_smooth(n);
    }

    /* As shown in the paper, it is sufficient to take
       N >= alpha n + 1 where alpha = 3/W(3/e) = 4.970625759544... */
    {
        fmpz_t a;
        fmpz_init(a);
        fmpz_set_ui(a, n);
        fmpz_mul_ui(a, a, 4970626);
        fmpz_cdiv_q_ui(a, a, 1000000);
        fmpz_add_ui(a, a, 1);
        N = fmpz_get_ui(a);
        fmpz_clear(a);
    }

    M = 2 * n;

    wp  = bits   + 2 * FLINT_BIT_COUNT(n);
    wp2 = bits/2 + 2 * FLINT_BIT_COUNT(n);

    euler_bsplit_init(sum);
    arb_init(P2);
    arb_init(T2);
    arb_init(Q2);
    arb_init(t);
    arb_init(u);
    arb_init(v);

    /* Compute S0 = V / (Q D), I0 = 1 + T / Q */
    euler_bsplit_1(sum, 0, N, n, wp, 0);

    /* I0 = T / Q */
    arb_add(sum->T, sum->T, sum->Q, wp);

    /* Compute S0 / I0 = V / (D T) */
    arb_mul(t, sum->T, sum->D, wp);
    arb_div(res, sum->V, t, wp);

    /* Compute K0 (actually I_0(2n) K_0(2n)) = T2 / Q2 */
    euler_bsplit_2(P2, Q2, T2, 0, M, n, wp2, 0);

    /* Compute K0 / I^2 = Q^2 * T2 / (Q2 * T^2) */
    arb_set_round(t, sum->Q, wp2);
    arb_mul(t, t, t, wp2);
    arb_mul(t, t, T2, wp2);
    arb_set_round(u, sum->T, wp2);
    arb_mul(u, u, u, wp2);
    arb_mul(u, u, Q2, wp2);
    arb_div(t, t, u, wp2);

    arb_sub(res, res, t, wp);

    /* subtract log(n) */
    arb_log_ui_smooth(u, n, wp);
    arb_sub(res, res, u, wp);

    /* add error bound 24 exp(-8n) */
    {
        mag_t b;
        mag_init(b);

        /* exp(-8) < 737690121 * 2^-41 */
        mag_set_ui_2exp_si(b, 737690121, -41);

        mag_pow_ui(b, b, n);
        mag_mul_ui(b, b, 24);

        mag_add(arb_radref(res), arb_radref(res), b);
        mag_clear(b);
    }

    arb_clear(P2);
    arb_clear(T2);
    arb_clear(Q2);
    arb_clear(t);
    arb_clear(u);
    arb_clear(v);

    euler_bsplit_clear(sum);
}

ARB_DEF_CACHED_CONSTANT(arb_const_euler, arb_const_euler_eval)
