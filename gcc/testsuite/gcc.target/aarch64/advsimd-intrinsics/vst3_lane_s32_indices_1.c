#include <arm_neon.h>

/* { dg-do compile } */
/* { dg-skip-if "" { *-*-* } { "-fno-fat-lto-objects" } } */
/* { dg-excess-errors "" { xfail arm*-*-* } } */

void
f_vst3_lane_s32 (int32_t * p, int32x2x3_t v)
{
  /* { dg-error "lane 2 out of range 0 - 1" "" { xfail arm*-*-* } 0 } */
  vst3_lane_s32 (p, v, 2);
  /* { dg-error "lane -1 out of range 0 - 1" "" { xfail arm*-*-* } 0 } */
  vst3_lane_s32 (p, v, -1);
  return;
}
