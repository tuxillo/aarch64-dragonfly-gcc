//===-- sanitizer_stacktrace.cc -------------------------------------------===//
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is shared between AddressSanitizer and ThreadSanitizer
// run-time libraries.
//===----------------------------------------------------------------------===//

#include "sanitizer_common.h"
#include "sanitizer_flags.h"
#include "sanitizer_stacktrace.h"

namespace __sanitizer {

uptr StackTrace::GetPreviousInstructionPc(uptr pc) {
#if defined(__arm__)
  // Cancel Thumb bit.
  pc = pc & (~1);
#endif
#if defined(__powerpc__) || defined(__powerpc64__)
  // PCs are always 4 byte aligned.
  return pc - 4;
#elif defined(__sparc__) || defined(__mips__)
  return pc - 8;
#else
  return pc - 1;
#endif
}

uptr StackTrace::GetCurrentPc() {
  return GET_CALLER_PC();
}

void BufferedStackTrace::Init(const uptr *pcs, uptr cnt, uptr extra_top_pc) {
  size = cnt + !!extra_top_pc;
  CHECK_LE(size, kStackTraceMax);
  internal_memcpy(trace_buffer, pcs, cnt * sizeof(trace_buffer[0]));
  if (extra_top_pc)
    trace_buffer[cnt] = extra_top_pc;
  top_frame_bp = 0;
}

// Check if given pointer points into allocated stack area.
static inline bool IsValidFrame(uptr frame, uptr stack_top, uptr stack_bottom) {
  return frame > stack_bottom && frame < stack_top - 2 * sizeof (uhwptr);
}

// In GCC on ARM bp points to saved lr, not fp, so we should check the next
// cell in stack to be a saved frame pointer. GetCanonicFrame returns the
// pointer to saved frame pointer in any case.
static inline uhwptr *GetCanonicFrame(uptr bp,
                                      uptr stack_top,
                                      uptr stack_bottom) {
#ifdef __arm__
  if (!IsValidFrame(bp, stack_top, stack_bottom)) return 0;
  uhwptr *bp_prev = (uhwptr *)bp;
  if (IsValidFrame((uptr)bp_prev[0], stack_top, stack_bottom)) return bp_prev;
  // The next frame pointer does not look right. This could be a GCC frame, step
  // back by 1 word and try again.
  if (IsValidFrame((uptr)bp_prev[-1], stack_top, stack_bottom))
    return bp_prev - 1;
  // Nope, this does not look right either. This means the frame after next does
  // not have a valid frame pointer, but we can still extract the caller PC.
  // Unfortunately, there is no way to decide between GCC and LLVM frame
  // layouts. Assume LLVM.
  return bp_prev;
#else
  return (uhwptr*)bp;
#endif
}

void BufferedStackTrace::FastUnwindStack(uptr pc, uptr bp, uptr stack_top,
                                         uptr stack_bottom, uptr max_depth) {
  CHECK_GE(max_depth, 2);
  trace_buffer[0] = pc;
  size = 1;
  if (stack_top < 4096) return;  // Sanity check for stack top.
  uhwptr *frame = GetCanonicFrame(bp, stack_top, stack_bottom);
  // Lowest possible address that makes sense as the next frame pointer.
  // Goes up as we walk the stack.
  uptr bottom = stack_bottom;
  // Avoid infinite loop when frame == frame[0] by using frame > prev_frame.
  while (IsValidFrame((uptr)frame, stack_top, bottom) &&
         IsAligned((uptr)frame, sizeof(*frame)) &&
         size < max_depth) {
#ifdef __powerpc__
    // PowerPC ABIs specify that the return address is saved at offset
    // 16 of the *caller's* stack frame.  Thus we must dereference the
    // back chain to find the caller frame before extracting it.
    uhwptr *caller_frame = (uhwptr*)frame[0];
    if (!IsValidFrame((uptr)caller_frame, stack_top, bottom) ||
	!IsAligned((uptr)caller_frame, sizeof(uhwptr)))
      break;
    uhwptr pc1 = caller_frame[2];
#else
    uhwptr pc1 = frame[1];
#endif
    if (pc1 != pc) {
      trace_buffer[size++] = (uptr) pc1;
    }
    bottom = (uptr)frame;
    frame = GetCanonicFrame((uptr)frame[0], stack_top, bottom);
  }
}

static bool MatchPc(uptr cur_pc, uptr trace_pc, uptr threshold) {
  return cur_pc - trace_pc <= threshold || trace_pc - cur_pc <= threshold;
}

void BufferedStackTrace::PopStackFrames(uptr count) {
  CHECK_LT(count, size);
  size -= count;
  for (uptr i = 0; i < size; ++i) {
    trace_buffer[i] = trace_buffer[i + count];
  }
}

uptr BufferedStackTrace::LocatePcInTrace(uptr pc) {
  // Use threshold to find PC in stack trace, as PC we want to unwind from may
  // slightly differ from return address in the actual unwinded stack trace.
  const int kPcThreshold = 288;
  for (uptr i = 0; i < size; ++i) {
    if (MatchPc(pc, trace[i], kPcThreshold))
      return i;
  }
  return 0;
}

}  // namespace __sanitizer
