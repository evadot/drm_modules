/**************************************************************************
 *
 * Copyright 2008-2009 VMware, Inc., Palo Alto, CA., USA
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
 */

#ifdef __linux__
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/module.h>
#endif
#include <drm/drmP.h>
#include <drm/drm_global.h>

#ifdef __FreeBSD__
MALLOC_DEFINE(M_DRM_GLOBAL, "drm_global", "DRM Global Items");
#endif

struct drm_global_item {
#ifdef FREEBSD_NOTYET
	struct mutex mutex;
#else
	struct sx mutex;
#endif
	void *object;
	int refcount;
};

static struct drm_global_item glob[DRM_GLOBAL_NUM];

void drm_global_init(void)
{
	int i;

	for (i = 0; i < DRM_GLOBAL_NUM; ++i) {
		struct drm_global_item *item = &glob[i];
#ifdef FREEBSD_NOTYET
		mutex_init(&item->mutex);
#else
		sx_init(&item->mutex, "drmgi");
#endif
		item->object = NULL;
		item->refcount = 0;
	}
}

void drm_global_release(void)
{
	int i;
	for (i = 0; i < DRM_GLOBAL_NUM; ++i) {
		struct drm_global_item *item = &glob[i];
#ifdef FREEBSD_NOTYET
		BUG_ON(item->object != NULL);
		BUG_ON(item->refcount != 0);
		mutex_destroy(&item->mutex);
#else
		MPASS(item->object == NULL);
		MPASS(item->refcount == 0);
		sx_destroy(&item->mutex);
#endif
	}
}

int drm_global_item_ref(struct drm_global_reference *ref)
{
	int ret;
	struct drm_global_item *item = &glob[ref->global_type];
	void *object;

#ifdef FREEBSD_NOTYET
	mutex_lock(&item->mutex);
#else
	sx_xlock(&item->mutex);
#endif
	if (item->refcount == 0) {
#ifdef FREEBSD_NOTYET
		item->object = kzalloc(ref->size, GFP_KERNEL);
#else
		item->object = malloc(ref->size, M_DRM_GLOBAL,
		    M_NOWAIT | M_ZERO);
#endif
		if (unlikely(item->object == NULL)) {
			ret = -ENOMEM;
			goto out_err;
		}

		ref->object = item->object;
		ret = ref->init(ref);
		if (unlikely(ret != 0))
			goto out_err;

	}
	++item->refcount;
	ref->object = item->object;
	object = item->object;
#ifdef FREEBSD_NOTYET
	mutex_unlock(&item->mutex);
#else
	sx_xunlock(&item->mutex);
#endif
	return 0;
out_err:
#ifdef FREEBSD_NOTYET
	mutex_unlock(&item->mutex);
#else
	sx_xunlock(&item->mutex);
#endif
	item->object = NULL;
	return ret;
}
EXPORT_SYMBOL(drm_global_item_ref);

void drm_global_item_unref(struct drm_global_reference *ref)
{
	struct drm_global_item *item = &glob[ref->global_type];

#ifdef FREEBSD_NOTYET
	mutex_lock(&item->mutex);
	BUG_ON(item->refcount == 0);
	BUG_ON(ref->object != item->object);
#else
	sx_xlock(&item->mutex);
	MPASS(item->refcount != 0);
	MPASS(ref->object == item->object);
#endif
	if (--item->refcount == 0) {
		ref->release(ref);
		free(item->object, M_DRM_GLOBAL);
		item->object = NULL;
	}
#ifdef FREEBSD_NOTYET
	mutex_unlock(&item->mutex);
#else
	sx_xunlock(&item->mutex);
#endif
}
EXPORT_SYMBOL(drm_global_item_unref);

