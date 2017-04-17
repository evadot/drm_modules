/**************************************************************************
 *
 * Copyright (c) 2006-2009 VMware, Inc., Palo Alto, CA., USA
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Authors: Thomas Hellstrom <thellstrom-at-vmware-dot-com>
 *          Keith Packard.
 */

#include <drm/ttm/ttm_module.h>
#include <drm/ttm/ttm_bo_driver.h>
#include <drm/ttm/ttm_page_alloc.h>
#ifdef TTM_HAS_AGP
#include <drm/ttm/ttm_placement.h>
#ifdef __linux__
#include <linux/agp_backend.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <asm/agp.h>
#endif

struct ttm_agp_backend {
	struct ttm_tt ttm;
#ifdef __linux__
	struct agp_memory *mem;
	struct agp_bridge_data *bridge;
#elif __FreeBSD__
	vm_offset_t offset;
	vm_page_t *pages;
	device_t bridge;
#endif
};

MALLOC_DEFINE(M_TTM_AGP, "ttm_agp", "TTM AGP Backend");

static int ttm_agp_bind(struct ttm_tt *ttm, struct ttm_mem_reg *bo_mem)
{
	struct ttm_agp_backend *agp_be = container_of(ttm, struct ttm_agp_backend, ttm);
	struct drm_mm_node *node = bo_mem->mm_node;
#ifdef __linux__
	struct agp_memory *mem;
	int ret, cached = (bo_mem->placement & TTM_PL_FLAG_CACHED);
#elif __FreeBSD__
	int ret;
#endif
	unsigned i;

#ifdef __linux__
	mem = agp_allocate_memory(agp_be->bridge, ttm->num_pages, AGP_USER_MEMORY);
	if (unlikely(mem == NULL))
		return -ENOMEM;

	mem->page_count = 0;
#endif
	for (i = 0; i < ttm->num_pages; i++) {
#ifdef __linux__
		struct page *page = ttm->pages[i];
#elif __FreeBSD__
		vm_page_t page = ttm->pages[i];
#endif

		if (!page)
			page = ttm->dummy_read_page;

#ifdef __linux__
		mem->pages[mem->page_count++] = page;
#elif __FreeBSD__
		agp_be->pages[i] = page;
#endif
	}
#ifdef __linux__
	agp_be->mem = mem;

	mem->is_flushed = 1;
	mem->type = (cached) ? AGP_USER_CACHED_MEMORY : AGP_USER_MEMORY;

	ret = agp_bind_memory(mem, node->start);
	if (ret)
		pr_err("AGP Bind memory failed\n");
#elif __FreeBSD__
	agp_be->offset = node->start * PAGE_SIZE;
	ret = -agp_bind_pages(agp_be->bridge, agp_be->pages,
			      ttm->num_pages << PAGE_SHIFT, agp_be->offset);
	if (ret)
		printf("[TTM] AGP Bind memory failed\n");
#endif

	return ret;
}

static int ttm_agp_unbind(struct ttm_tt *ttm)
{
	struct ttm_agp_backend *agp_be = container_of(ttm, struct ttm_agp_backend, ttm);

#ifdef __linux__
	if (agp_be->mem) {
		if (agp_be->mem->is_bound)
			return agp_unbind_memory(agp_be->mem);
		agp_free_memory(agp_be->mem);
		agp_be->mem = NULL;
	}
	return 0;
#elif __FreeBSD__
	return -agp_unbind_pages(agp_be->bridge, ttm->num_pages << PAGE_SHIFT,
				 agp_be->offset);
#endif
}

static void ttm_agp_destroy(struct ttm_tt *ttm)
{
	struct ttm_agp_backend *agp_be = container_of(ttm, struct ttm_agp_backend, ttm);

#ifdef __linux__
	if (agp_be->mem)
		ttm_agp_unbind(ttm);
	ttm_tt_fini(ttm);
	kfree(agp_be);
#elif __FreeBSD__
	ttm_tt_fini(ttm);
	free(agp_be->pages, M_TTM_AGP);
	free(agp_be, M_TTM_AGP);
#endif
}

static struct ttm_backend_func ttm_agp_func = {
	.bind = ttm_agp_bind,
	.unbind = ttm_agp_unbind,
	.destroy = ttm_agp_destroy,
};

struct ttm_tt *ttm_agp_tt_create(struct ttm_bo_device *bdev,
				 device_t bridge,
				 unsigned long size, uint32_t page_flags,
				 vm_page_t dummy_read_page)
{
	struct ttm_agp_backend *agp_be;

	agp_be = malloc(sizeof(*agp_be), M_TTM_AGP, M_WAITOK | M_ZERO);

	agp_be->bridge = bridge;
	agp_be->ttm.func = &ttm_agp_func;

	if (ttm_tt_init(&agp_be->ttm, bdev, size, page_flags, dummy_read_page)) {
		free(agp_be, M_TTM_AGP);
		return NULL;
	}

	agp_be->offset = 0;
	agp_be->pages = malloc(agp_be->ttm.num_pages * sizeof(*agp_be->pages),
			       M_TTM_AGP, M_WAITOK);

	return &agp_be->ttm;
}

int ttm_agp_tt_populate(struct ttm_tt *ttm)
{
	if (ttm->state != tt_unpopulated)
		return 0;

	return ttm_pool_populate(ttm);
}

void ttm_agp_tt_unpopulate(struct ttm_tt *ttm)
{
	ttm_pool_unpopulate(ttm);
}

#endif
