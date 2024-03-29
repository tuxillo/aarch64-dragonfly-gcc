#include <arm_neon.h>

/* { dg-do compile } */
/* { dg-skip-if "" { *-*-* } { "-fno-fat-lto-objects" } } */
/* { dg-excess-errors "" { xfail arm*-*-* } } */
/* { dg-skip-if "" { arm*-*-* } } */

poly8x16x3_t
f_vld3q_lane_p8 (poly8_t * p, poly8x16x3_t v)
{
  poly8x16x3_t res;
  /* { dg-error "lane 16 out of range 0 - 15" "" { xfail arm*-*-* } 0 } */
  res = vld3q_lane_p8 (p, v, 16);
  /* { dg-error "lane -1 out of range 0 - 15" "" { xfail arm*-*-* } 0 } */
  res = vld3q_lane_p8 (p, v, -1);
  return res;
}
