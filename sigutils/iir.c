/*

  Copyright (C) 2016 Gonzalo José Carracedo Carballal

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program.  If not, see
  <http://www.gnu.org/licenses/>

*/

#include <stdlib.h>
#include <string.h>

#include "sampling.h"
#include "iir.h"
#include "coef.h"
#include "taps.h"

SUPRIVATE void
__su_iir_filt_push_x(su_iir_filt_t *filt, SUCOMPLEX x)
{
  filt->x[filt->x_ptr++] = x;
  if (filt->x_ptr >= filt->x_size)
    filt->x_ptr = 0;
}

SUPRIVATE void
__su_iir_filt_push_y(su_iir_filt_t *filt, SUCOMPLEX y)
{
  if (filt->y_size > 0) {
    filt->y[filt->y_ptr++] = y;
    if (filt->y_ptr >= filt->y_size)
      filt->y_ptr = 0;
  }
}

SUPRIVATE SUCOMPLEX
__su_iir_filt_eval(const su_iir_filt_t *filt)
{
  unsigned int i;
  int p;

  SUCOMPLEX y = 0;

  /* Input feedback */
  p = filt->x_ptr - 1;
  for (i = 0; i < filt->x_size; ++i) {
    if (p < 0)
      p += filt->x_size;

    y += filt->b[i] * filt->x[p--];
  }

  if (filt->y_size > 0) {
    /* Output feedback - assumes that a[0] is 1 */
    p = filt->y_ptr - 1;
    for (i = 1; i < filt->y_size; ++i) {
      if (p < 0)
        p += filt->y_size;

      y -= filt->a[i] * filt->y[p--];
    }
  }

  return y;
}

void
su_iir_filt_finalize(su_iir_filt_t *filt)
{
  if (filt->a != NULL)
    free(filt->a);

  if (filt->b != NULL)
    free(filt->b);

  if (filt->x != NULL)
      free(filt->x);

  if (filt->y != NULL)
    free(filt->y);
}

SUCOMPLEX
su_iir_filt_feed(su_iir_filt_t *filt, SUCOMPLEX x)
{
  SUCOMPLEX y;

  __su_iir_filt_push_x(filt, x);
  y = __su_iir_filt_eval(filt);
  __su_iir_filt_push_y(filt, y);

  filt->curr_y = y;
  return filt->gain * y;
}

SUCOMPLEX
su_iir_filt_get(const su_iir_filt_t *filt)
{
  return filt->gain * filt->curr_y;
}

void
su_iir_filt_set_gain(su_iir_filt_t *filt, SUFLOAT gain)
{
  filt->gain = gain;
}

SUBOOL
__su_iir_filt_init(
    su_iir_filt_t *filt,
    SUSCOUNT y_size,
    SUFLOAT *a,
    SUSCOUNT x_size,
    SUFLOAT *b,
    SUBOOL copy_coef)
{
  SUCOMPLEX *x = NULL;
  SUCOMPLEX *y = NULL;
  SUFLOAT *a_copy = NULL;
  SUFLOAT *b_copy = NULL;

  memset(filt, 0, sizeof (su_iir_filt_t));

  filt->gain = 1;

  if ((x = calloc(x_size, sizeof (SUCOMPLEX))) == NULL)
    goto fail;

  if (y_size > 0)
    if ((y = calloc(y_size, sizeof (SUCOMPLEX))) == NULL)
      goto fail;

  if (copy_coef) {
    if (y_size > 0) {
      if ((a_copy = malloc(y_size * sizeof (SUFLOAT))) == NULL)
        goto fail;

      memcpy(a_copy, a, y_size * sizeof (SUFLOAT));
    }

    if ((b_copy = malloc(x_size * sizeof (SUFLOAT))) == NULL)
      goto fail;

    memcpy(b_copy, b, x_size * sizeof (SUFLOAT));
  } else {
    a_copy = a;
    b_copy = b;
  }

  filt->x = x;
  filt->y = y;

  filt->a = a_copy;
  filt->b = b_copy;

  filt->x_ptr = 0;
  filt->y_ptr = 0;

  filt->x_size = x_size;
  filt->y_size = y_size;

  return SU_TRUE;

fail:
  if (x != NULL)
    free(x);

  if (y != NULL)
    free(y);

  if (copy_coef) {
    if (a_copy != NULL)
      free(a_copy);

    if (b_copy != NULL)
      free(b_copy);
  }

  return SU_FALSE;
}

SUBOOL
su_iir_filt_init(
    su_iir_filt_t *filt,
    SUSCOUNT y_size,
    const SUFLOAT *a,
    SUSCOUNT x_size,
    const SUFLOAT *b)
{
  return __su_iir_filt_init(
      filt,
      y_size,
      (SUFLOAT *) a,
      x_size,
      (SUFLOAT *) b,
      SU_TRUE);
}

SUBOOL
su_iir_bwlpf_init(su_iir_filt_t *filt, SUSCOUNT n, SUFLOAT fc)
{
  SUFLOAT *a = NULL;
  SUFLOAT *b = NULL;
  SUFLOAT scaling;

  unsigned int i;

  if ((a = su_dcof_bwlp(n, fc)) == NULL)
    goto fail;

  if ((b = su_ccof_bwlp(n)) == NULL)
    goto fail;

  scaling = su_sf_bwlp(n, fc);

  for (i = 0; i < n + 1; ++i)
    b[i] *= scaling;

  if (!__su_iir_filt_init(filt, n + 1, a, n + 1, b, SU_FALSE))
    goto fail;

  return SU_TRUE;

fail:
  if (a != NULL)
    free(a);

  if (b != NULL)
    free(b);

  return SU_FALSE;
}

SUBOOL
su_iir_bwbpf_init(su_iir_filt_t *filt, SUSCOUNT n, SUFLOAT f1, SUFLOAT f2)
{
  SUFLOAT *a = NULL;
  SUFLOAT *b = NULL;
  SUFLOAT scaling;

  unsigned int i;

  if ((a = su_dcof_bwbp(n, f1, f2)) == NULL)
    goto fail;

  if ((b = su_ccof_bwbp(n)) == NULL)
    goto fail;

  scaling = su_sf_bwbp(n, f1, f2);

  for (i = 0; i < n + 1; ++i) {
    b[i] *= scaling;
  }

  if (!__su_iir_filt_init(filt, 2 * n + 1, a, 2 * n + 1, b, SU_FALSE))
    goto fail;

  return SU_TRUE;

fail:
  if (a != NULL)
    free(a);

  if (b != NULL)
    free(b);

  return SU_FALSE;
}

SUBOOL
su_iir_rrc_init(su_iir_filt_t *filt, SUSCOUNT n, SUFLOAT T, SUFLOAT beta)
{
  SUFLOAT *b = NULL;
  unsigned int i;

  if (n < 1)
    goto fail;

  if ((b = malloc(n * sizeof (SUFLOAT))) == NULL)
    goto fail;

  su_taps_rrc_init(b, T, beta, n);

  if (!__su_iir_filt_init(filt, 0, NULL, n, b, SU_FALSE))
    goto fail;

  return SU_TRUE;

fail:
  if (b != NULL)
    free(b);

  return SU_FALSE;
}

SUBOOL
su_iir_brickwall_bp_init(
    su_iir_filt_t *filt,
    SUSCOUNT n,
    SUFLOAT bw,
    SUFLOAT ifnor)
{
  SUFLOAT *b = NULL;
  unsigned int i;

  if (n < 1)
    goto fail;

  if ((b = malloc(n * sizeof (SUFLOAT))) == NULL)
    goto fail;

  su_taps_brickwall_bp_init(b, bw, ifnor, n);

  if (!__su_iir_filt_init(filt, 0, NULL, n, b, SU_FALSE))
    goto fail;

  return SU_TRUE;

fail:
  if (b != NULL)
    free(b);

  return SU_FALSE;
}

SUBOOL
su_iir_brickwall_lp_init(su_iir_filt_t *filt, SUSCOUNT n, SUFLOAT fc)
{
  SUFLOAT *b = NULL;
  unsigned int i;

  if (n < 1)
    goto fail;

  if ((b = malloc(n * sizeof (SUFLOAT))) == NULL)
    goto fail;

  su_taps_brickwall_lp_init(b, fc, n);

  if (!__su_iir_filt_init(filt, 0, NULL, n, b, SU_FALSE))
    goto fail;

  return SU_TRUE;

fail:
  if (b != NULL)
    free(b);

  return SU_FALSE;
}
