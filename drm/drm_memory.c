/**
 * \file drm_memory.c
 * Memory management wrappers for DRM
 *
 * \author Rickard E. (Rik) Faith <faith@valinux.com>
 * \author Gareth Hughes <gareth@valinux.com>
 */

/*
 * Created: Thu Feb  4 14:00:34 1999 by faith@valinux.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifdef __linux__
#include <linux/highmem.h>
#endif
#include <linux/export.h>
#include <drm/drmP.h>

#define	vunmap(handle)

#if __OS_HAS_AGP
static void *agp_remap(unsigned long offset, unsigned long size,
		       struct drm_device * dev)
{
#ifdef __linux__
	unsigned long i, num_pages =
	    PAGE_ALIGN(size) / PAGE_SIZE;
	struct drm_agp_mem *agpmem;
	struct page **page_map;
	struct page **phys_page_map;
	void *addr;

	size = PAGE_ALIGN(size);

#ifdef __alpha__
	offset -= dev->hose->mem_space->start;
#endif

	list_for_each_entry(agpmem, &dev->agp->memory, head)
		if (agpmem->bound <= offset
		    && (agpmem->bound + (agpmem->pages << PAGE_SHIFT)) >=
		    (offset + size))
			break;
	if (&agpmem->head == &dev->agp->memory)
		return NULL;

	/*
	 * OK, we're mapping AGP space on a chipset/platform on which memory accesses by
	 * the CPU do not get remapped by the GART.  We fix this by using the kernel's
	 * page-table instead (that's probably faster anyhow...).
	 */
	/* note: use vmalloc() because num_pages could be large... */
	page_map = vmalloc(num_pages * sizeof(struct page *));
	if (!page_map)
		return NULL;

	phys_page_map = (agpmem->memory->pages + (offset - agpmem->bound) / PAGE_SIZE);
	for (i = 0; i < num_pages; ++i)
		page_map[i] = phys_page_map[i];
	addr = vmap(page_map, num_pages, VM_IOREMAP, PAGE_AGP);
	vfree(page_map);

	return addr;
#elif __FreeBSD__
	/*
	 * FIXME Linux<->FreeBSD: Not implemented. This is never called
	 * on FreeBSD anyway, because drm_agp_mem->cant_use_aperture is
	 * set to 0.
	 */
	return NULL;
#endif
}

/** Wrapper around agp_free_memory() */
void drm_free_agp(DRM_AGP_MEM * handle, int pages)
{
#ifdef __linux__
	agp_free_memory(handle);
#elif __FreeBSD__
	device_t agpdev;

	agpdev = agp_find_device();
	if (!agpdev || !handle)
		return;

	agp_free_memory(agpdev, handle);
#endif
}
EXPORT_SYMBOL(drm_free_agp);

/** Wrapper around agp_bind_memory() */
int drm_bind_agp(DRM_AGP_MEM * handle, unsigned int start)
{
#ifdef __linux__
	return agp_bind_memory(handle, start);
#elif __FreeBSD__
	device_t agpdev;

	agpdev = agp_find_device();
	if (!agpdev || !handle)
		return -EINVAL;

	return -agp_bind_memory(agpdev, handle, start * PAGE_SIZE);
#endif
}

/** Wrapper around agp_unbind_memory() */
int drm_unbind_agp(DRM_AGP_MEM * handle)
{
#ifdef __linux__
	return agp_unbind_memory(handle);
#elif __FreeBSD__
	device_t agpdev;

	agpdev = agp_find_device();
	if (!agpdev || !handle)
		return -EINVAL;

	return -agp_unbind_memory(agpdev, handle);
#endif
}
EXPORT_SYMBOL(drm_unbind_agp);

#else  /*  __OS_HAS_AGP  */
static inline void *agp_remap(unsigned long offset, unsigned long size,
			      struct drm_device * dev)
{
	return NULL;
}

#endif				/* agp */

void drm_core_ioremap(struct drm_local_map *map, struct drm_device *dev)
{
	if (drm_core_has_AGP(dev) &&
	    dev->agp && dev->agp->cant_use_aperture && map->type == _DRM_AGP)
		map->handle = agp_remap(map->offset, map->size, dev);
	else
#ifdef __linux__
		map->handle = ioremap(map->offset, map->size);
#elif __FreeBSD__
		map->handle = pmap_mapdev(map->offset, map->size);
#endif
}
EXPORT_SYMBOL(drm_core_ioremap);

void drm_core_ioremap_wc(struct drm_local_map *map, struct drm_device *dev)
{
	if (drm_core_has_AGP(dev) &&
	    dev->agp && dev->agp->cant_use_aperture && map->type == _DRM_AGP)
		map->handle = agp_remap(map->offset, map->size, dev);
	else
#ifdef __linux__
		map->handle = ioremap_wc(map->offset, map->size);
#elif __FreeBSD__
		map->handle = pmap_mapdev_attr(map->offset, map->size,
		    VM_MEMATTR_WRITE_COMBINING);
#endif
}
EXPORT_SYMBOL(drm_core_ioremap_wc);

void drm_core_ioremapfree(struct drm_local_map *map, struct drm_device *dev)
{
	if (!map->handle || !map->size)
		return;

	if (drm_core_has_AGP(dev) &&
	    dev->agp && dev->agp->cant_use_aperture && map->type == _DRM_AGP)
		vunmap(map->handle);
	else
#ifdef __linux__
		iounmap(map->handle);
#elif __FreeBSD__
		pmap_unmapdev((vm_offset_t)map->handle, map->size);
#endif
}
EXPORT_SYMBOL(drm_core_ioremapfree);
