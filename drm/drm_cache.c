#include <drm/drmP.h>

void
drm_clflush_pages(vm_page_t *pages, unsigned long num_pages)
{

#if defined(__i386__) || defined(__amd64__)
	pmap_invalidate_cache_pages(pages, num_pages);
#else
	DRM_ERROR("drm_clflush_pages not implemented on this architecture");
#endif
}

void
drm_clflush_virt_range(char *addr, unsigned long length)
{

#if defined(__i386__) || defined(__amd64__)
	pmap_invalidate_cache_range((vm_offset_t)addr,
	    (vm_offset_t)addr + length, TRUE);
#else
	DRM_ERROR("drm_clflush_virt_range not implemented on this architecture");
#endif
}

void
drm_clflush_sg(struct sg_table *st)
{
	struct page *page;
	struct scatterlist *sg;
	int i;

	for_each_sg(st->sgl, sg, st->nents, i) {
		page = sg_page(sg);
		drm_clflush_pages(&page, 1);
	}
}
