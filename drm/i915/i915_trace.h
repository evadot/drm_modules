/*-
 * Copyright (c) 2017 Emmanuel Vadot <manu@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef __I915_TRACE_H_
#define __I915_TRACE_H_

static inline void
trace_i915_reg_rw(boolean_t rw, int reg, uint64_t val, int sz)
{

	CTR4(KTR_DRM_REG, "[%x/%d] %c %x", reg, sz, rw ? "w" : "r", val);
}

static inline void
trace_i915_gem_request_wait_begin(struct intel_ring_buffer *ring, int seqno)
{

	CTR2(KTR_DRM, "request_wait_begin %s %d", ring->name, seqno);
}

static inline void
trace_i915_gem_request_wait_end(struct intel_ring_buffer *ring, int seqno)
{

	CTR2(KTR_DRM, "request_wait_end %s %d", ring->name, seqno);
}

static inline void
trace_i915_gem_request_complete(struct intel_ring_buffer *ring, int seqno)
{

	CTR2(KTR_DRM, "request_complete %s %d", ring->name, ring->get_seqno(ring, false));
}

static inline void
trace_i915_gem_request_add(struct intel_ring_buffer *ring, int seqno)
{

	CTR2(KTR_DRM, "request_add %s %d", ring->name, seqno);
}

static inline void
trace_i915_gem_request_retire(struct intel_ring_buffer *ring, int seqno)
{
	CTR2(KTR_DRM, "retire_request_seqno_passed %s %d", ring->name, seqno);
}

static inline void
trace_i915_gem_object_bind(struct drm_i915_gem_object *obj, int map_and_fenceable)
{
	CTR4(KTR_DRM, "object_bind %p %x %x %d", obj, obj->gtt_offset, obj->base.size, map_and_fenceable);
}

static inline void
trace_i915_gem_object_unbind(struct drm_i915_gem_object *obj)
{
	CTR1(KTR_DRM, "object_unbind %p", obj);
}

static inline void
trace_i915_gem_object_create(struct drm_i915_gem_object *obj)
{

	CTR2(KTR_DRM, "object_create %p %x", obj, obj->size);
}

static inline void
trace_i915_gem_object_destroy(struct drm_i915_gem_object *obj)
{

	CTR2(KTR_DRM, "object_destroy %p %x", obj, obj->size);
}

static inline void
trace_i915_gem_object_clflush(struct drm_i915_gem_object *obj)
{

	CTR1(KTR_DRM, "object_clflush %p %x", obj);
}

static inline void
trace_i915_gem_object_pread(struct drm_i915_gem_object *obj, unsigned long offset, unsigned long size)
{

	CTR3(KTR_DRM, "pread %p %jx %jx", obj, offset, size);
}

static inline void
trace_i915_gem_object_pwrite(struct drm_i915_gem_object *obj, unsigned long offset, unsigned long size)
{

	CTR3(KTR_DRM, "pwrite %p %jx %jx", obj, offset, size);
}

static inline void
trace_i915_gem_object_change_domain(struct drm_i915_gem_object *obj, u32 old_read, u32 old_write)
{

		CTR3(KTR_DRM, "object_change_domain %p %x %x",
		    obj, old_read, old_write);
}

static inline void
trace_i915_gem_evict(struct drm_device *dev, int min_size,
  unsigned alignment, bool mappable)
{

	CTR4(KTR_DRM, "evict_something %p %d %u %d", dev, min_size,
	    alignment, mappable);
}

static inline void
trace_i915_gem_evict_everything(struct drm_device *dev)
{

	CTR1(KTR_DRM, "evict_everything %p", dev);
}

static inline void
trace_i915_gem_ring_dispatch(struct intel_ring_buffer *ring, int seqno, u32 flags)
{

	CTR3(KTR_DRM, "ring_dispatch ring=%s seqno=%d flags=%u", ring->name,
	    seqno, flags);
}

#endif /* _I915_TRACE_H_ */
