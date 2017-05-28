/*-
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Konstantin Belousov under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#include <drm/drmP.h>
#include <drm/drm.h>
#include <drm/drm_sarea.h>

/** @file drm_gem.c
 *
 * This file provides some of the base ioctls and library routines for
 * the graphics memory manager implemented by each device driver.
 *
 * Because various devices have different requirements in terms of
 * synchronization and migration strategies, implementing that is left up to
 * the driver, and all that the general API provides should be generic --
 * allocating objects, reading/writing data with the cpu, freeing objects.
 * Even there, platform-dependent optimizations for reading/writing data with
 * the CPU mean we'll likely hook those out to driver-specific calls.  However,
 * the DRI2 implementation wants to have at least allocate/mmap be generic.
 *
 * The goal was to have swap-backed object allocation managed through
 * struct file.  However, file descriptors as handles to a struct file have
 * two major failings:
 * - Process limits prevent more than 1024 or so being used at a time by
 *   default.
 * - Inability to allocate high fds will aggravate the X Server's select()
 *   handling, and likely that of many GL client applications as well.
 *
 * This led to a plan of using our own integer IDs (called handles, following
 * DRM terminology) to mimic fds, and implement the fd syscalls we need as
 * ioctls.  The objects themselves will still include the struct file so
 * that we can transition to fds if the required kernel infrastructure shows
 * up at a later date, and as our interface with shmfs for memory allocation.
 */

/*
 * We make up offsets for buffer objects so we can recognize them at
 * mmap time.
 */

/* pgoff in mmap is an unsigned long, so we need to make sure that
 * the faked up offset will fit
 */

#if BITS_PER_LONG == 64
#define DRM_FILE_PAGE_OFFSET_START ((0xFFFFFFFFUL >> PAGE_SHIFT) + 1)
#define DRM_FILE_PAGE_OFFSET_SIZE ((0xFFFFFFFFUL >> PAGE_SHIFT) * 16)
#else
#define DRM_FILE_PAGE_OFFSET_START ((0xFFFFFFFUL >> PAGE_SHIFT) + 1)
#define DRM_FILE_PAGE_OFFSET_SIZE ((0xFFFFFFFUL >> PAGE_SHIFT) * 16)
#endif

/**
 * Initialize the GEM device fields
 */

int
drm_gem_init(struct drm_device *dev)
{
	struct drm_gem_mm *mm;

	spin_lock_init(&dev->object_name_lock);
	idr_init(&dev->object_name_idr);

	mm = kzalloc(sizeof(struct drm_gem_mm), GFP_KERNEL);
	if (!mm) {
		DRM_ERROR("out of memory\n");
		return -ENOMEM;
	}

	dev->mm_private = mm;

	if (drm_ht_create(&mm->offset_hash, 12)) {
		kfree(mm);
		return -ENOMEM;
	}

#ifdef FREEBSD_NOTYET
	if (drm_mm_init(&mm->offset_manager, DRM_FILE_PAGE_OFFSET_START,
			DRM_FILE_PAGE_OFFSET_SIZE)) {
		drm_ht_remove(&mm->offset_hash);
		kfree(mm);
		return -ENOMEM;
	}
#else
	mm->idxunr = new_unrhdr(0, DRM_GEM_MAX_IDX, NULL);
#endif

	return 0;
}

void
drm_gem_destroy(struct drm_device *dev)
{
	struct drm_gem_mm *mm = dev->mm_private;

#ifdef FREEBSD_NOTYET
	drm_mm_takedown(&mm->offset_manager);
#endif
	drm_ht_remove(&mm->offset_hash);
#ifdef __FreeBSD__
	delete_unrhdr(mm->idxunr);
#endif
	kfree(mm);
	dev->mm_private = NULL;
}

/**
 * Initialize an already allocated GEM object of the specified size with
 * shmfs backing store.
 */
int drm_gem_object_init(struct drm_device *dev,
			struct drm_gem_object *obj, size_t size)
{
	BUG_ON((size & (PAGE_SIZE - 1)) != 0);

	obj->dev = dev;
#ifdef __linux__
	obj->filp = shmem_file_setup("drm mm object", size, VM_NORESERVE);
	if (IS_ERR(obj->filp))
		return PTR_ERR(obj->filp);
#elif __FreeBSD__
	obj->vm_obj = vm_pager_allocate(OBJT_DEFAULT, NULL, size,
	    VM_PROT_READ | VM_PROT_WRITE, 0, curthread->td_ucred);
#endif

	kref_init(&obj->refcount);
	atomic_set(&obj->handle_count, 0);
	obj->size = size;

	return 0;
}
EXPORT_SYMBOL(drm_gem_object_init);

/**
 * Initialize an already allocated GEM object of the specified size with
 * no GEM provided backing store. Instead the caller is responsible for
 * backing the object and handling it.
 */
int drm_gem_private_object_init(struct drm_device *dev,
			struct drm_gem_object *obj, size_t size)
{
	BUG_ON((size & (PAGE_SIZE - 1)) != 0);

	obj->dev = dev;
#ifdef __linux__
	obj->filp = NULL;
#elif __FreeBSD__
	obj->vm_obj = NULL;
#endif

	kref_init(&obj->refcount);
	atomic_set(&obj->handle_count, 0);
	obj->size = size;

	return 0;
}
EXPORT_SYMBOL(drm_gem_private_object_init);

/**
 * Allocate a GEM object of the specified size with shmfs backing store
 */
struct drm_gem_object *
drm_gem_object_alloc(struct drm_device *dev, size_t size)
{
	struct drm_gem_object *obj;

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		goto free;

	if (drm_gem_object_init(dev, obj, size) != 0)
		goto free;

	if (dev->driver->gem_init_object != NULL &&
	    dev->driver->gem_init_object(obj) != 0) {
		goto fput;
	}
	return obj;
fput:
#ifdef FREEBSD_NOTYET
	/* Object_init mangles the global counters - readjust them. */
	fput(obj->filp);
#else
	vm_object_deallocate(obj->vm_obj);
#endif
free:
	kfree(obj);
	return NULL;
}
EXPORT_SYMBOL(drm_gem_object_alloc);

#if defined(FREEBSD_NOTYET)
static void
drm_gem_remove_prime_handles(struct drm_gem_object *obj, struct drm_file *filp)
{
	if (obj->import_attach) {
		drm_prime_remove_buf_handle(&filp->prime,
				obj->import_attach->dmabuf);
	}
	if (obj->export_dma_buf) {
		drm_prime_remove_buf_handle(&filp->prime,
				obj->export_dma_buf);
	}
}
#endif

/**
 * Removes the mapping from handle to filp for this object.
 */
int
drm_gem_handle_delete(struct drm_file *filp, u32 handle)
{
	struct drm_device *dev;
	struct drm_gem_object *obj;

	/* This is gross. The idr system doesn't let us try a delete and
	 * return an error code.  It just spews if you fail at deleting.
	 * So, we have to grab a lock around finding the object and then
	 * doing the delete on it and dropping the refcount, or the user
	 * could race us to double-decrement the refcount and cause a
	 * use-after-free later.  Given the frequency of our handle lookups,
	 * we may want to use ida for number allocation and a hash table
	 * for the pointers, anyway.
	 */
	spin_lock(&filp->table_lock);

	/* Check if we currently have a reference on the object */
	obj = idr_find(&filp->object_idr, handle);
	if (obj == NULL) {
		spin_unlock(&filp->table_lock);
		return -EINVAL;
	}
	dev = obj->dev;

	/* Release reference and decrement refcount. */
	idr_remove(&filp->object_idr, handle);
	spin_unlock(&filp->table_lock);

#ifdef FREEBSD_NOTYET
	drm_gem_remove_prime_handles(obj, filp);
#endif

	if (dev->driver->gem_close_object)
		dev->driver->gem_close_object(obj, filp);
	drm_gem_object_handle_unreference_unlocked(obj);

	return 0;
}
EXPORT_SYMBOL(drm_gem_handle_delete);

/**
 * Create a handle for this object. This adds a handle reference
 * to the object, which includes a regular reference count. Callers
 * will likely want to dereference the object afterwards.
 */
int
drm_gem_handle_create(struct drm_file *file_priv,
		       struct drm_gem_object *obj,
		       u32 *handlep)
{
	struct drm_device *dev = obj->dev;
	int ret;

	/*
	 * Get the user-visible handle using idr.  Preload and perform
	 * allocation under our spinlock.
	 */
	idr_preload(GFP_KERNEL);
	spin_lock(&file_priv->table_lock);

	ret = idr_alloc(&file_priv->object_idr, obj, 1, 0, GFP_NOWAIT);

	spin_unlock(&file_priv->table_lock);
	idr_preload_end();
	if (ret < 0)
		return ret;
	*handlep = ret;

	drm_gem_object_handle_reference(obj);

	if (dev->driver->gem_open_object) {
		ret = dev->driver->gem_open_object(obj, file_priv);
		if (ret) {
			drm_gem_handle_delete(file_priv, *handlep);
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL(drm_gem_handle_create);


/**
 * drm_gem_free_mmap_offset - release a fake mmap offset for an object
 * @obj: obj in question
 *
 * This routine frees fake offsets allocated by drm_gem_create_mmap_offset().
 */
void
drm_gem_free_mmap_offset(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct drm_gem_mm *mm = dev->mm_private;
#ifdef __linux__
	struct drm_map_list *list = &obj->map_list;

	drm_ht_remove_item(&mm->offset_hash, &list->hash);
	drm_mm_put_block(list->file_offset_node);
	kfree(list->map);
	list->map = NULL;
#elif __FreeBSD__
	struct drm_hash_item *list = &obj->map_list;

	if (!obj->on_map)
		return;

	drm_ht_remove_item(&mm->offset_hash, list);
	free_unr(mm->idxunr, list->key);
	obj->on_map = false;
#endif
}
EXPORT_SYMBOL(drm_gem_free_mmap_offset);

/**
 * drm_gem_create_mmap_offset - create a fake mmap offset for an object
 * @obj: obj in question
 *
 * GEM memory mapping works by handing back to userspace a fake mmap offset
 * it can use in a subsequent mmap(2) call.  The DRM core code then looks
 * up the object based on the offset and sets up the various memory mapping
 * structures.
 *
 * This routine allocates and attaches a fake offset for @obj.
 */
int
drm_gem_create_mmap_offset(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct drm_gem_mm *mm = dev->mm_private;
#ifdef __linux__
	struct drm_map_list *list;
	struct drm_local_map *map;
#endif
	int ret;

#ifdef __linux__
	/* Set the object up for mmap'ing */
	list = &obj->map_list;
	list->map = kzalloc(sizeof(struct drm_map_list), GFP_KERNEL);
	if (!list->map)
		return -ENOMEM;

	map = list->map;
	map->type = _DRM_GEM;
	map->size = obj->size;
	map->handle = obj;

	/* Get a DRM GEM mmap offset allocated... */
	list->file_offset_node = drm_mm_search_free(&mm->offset_manager,
			obj->size / PAGE_SIZE, 0, false);

	if (!list->file_offset_node) {
		DRM_ERROR("failed to allocate offset for bo %d\n", obj->name);
		ret = -ENOSPC;
		goto out_free_list;
	}

	list->file_offset_node = drm_mm_get_block(list->file_offset_node,
			obj->size / PAGE_SIZE, 0);
	if (!list->file_offset_node) {
		ret = -ENOMEM;
		goto out_free_list;
	}

	list->hash.key = list->file_offset_node->start;
	ret = drm_ht_insert_item(&mm->offset_hash, &list->hash);
	if (ret) {
		DRM_ERROR("failed to add to map hash\n");
		goto out_free_mm;
	}
#elif __FreeBSD__
	if (obj->on_map)
		return 0;

	obj->map_list.key = alloc_unr(mm->idxunr);
	ret = drm_ht_insert_item(&mm->offset_hash, &obj->map_list);
	if (ret) {
		DRM_ERROR("failed to add to map hash\n");
		free_unr(mm->idxunr, obj->map_list.key);
		return ret;
	}
	obj->on_map = true;
#endif

	return 0;

#ifdef __linux__
out_free_mm:
	drm_mm_put_block(list->file_offset_node);
out_free_list:
	kfree(list->map);
	list->map = NULL;

	return ret;
#endif
}
EXPORT_SYMBOL(drm_gem_create_mmap_offset);

/** Returns a reference to the object named by the handle. */
struct drm_gem_object *
drm_gem_object_lookup(struct drm_device *dev, struct drm_file *filp,
		      u32 handle)
{
	struct drm_gem_object *obj;

	spin_lock(&filp->table_lock);

	/* Check if we currently have a reference on the object */
	obj = idr_find(&filp->object_idr, handle);
	if (obj == NULL) {
		spin_unlock(&filp->table_lock);
		return NULL;
	}

	drm_gem_object_reference(obj);

	spin_unlock(&filp->table_lock);

	return obj;
}
EXPORT_SYMBOL(drm_gem_object_lookup);

/**
 * Releases the handle to an mm object.
 */
int
drm_gem_close_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_gem_close *args = data;
	int ret;

	if (!(dev->driver->driver_features & DRIVER_GEM))
		return -ENODEV;

	ret = drm_gem_handle_delete(file_priv, args->handle);

	return ret;
}

/**
 * Create a global name for an object, returning the name.
 *
 * Note that the name does not hold a reference; when the object
 * is freed, the name goes away.
 */
int
drm_gem_flink_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_gem_flink *args = data;
	struct drm_gem_object *obj;
	int ret;

	if (!(dev->driver->driver_features & DRIVER_GEM))
		return -ENODEV;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return -ENOENT;

	idr_preload(GFP_KERNEL);
	spin_lock(&dev->object_name_lock);
	if (!obj->name) {
		ret = idr_alloc(&dev->object_name_idr, obj, 1, 0, GFP_NOWAIT);
		obj->name = ret;
		args->name = (uint64_t) obj->name;
		spin_unlock(&dev->object_name_lock);
		idr_preload_end();

		if (ret < 0)
			goto err;
		ret = 0;

		/* Allocate a reference for the name table.  */
		drm_gem_object_reference(obj);
	} else {
		args->name = (uint64_t) obj->name;
		spin_unlock(&dev->object_name_lock);
		idr_preload_end();
		ret = 0;
	}

err:
	drm_gem_object_unreference_unlocked(obj);
	return ret;
}

/**
 * Open an object using the global name, returning a handle and the size.
 *
 * This handle (of course) holds a reference to the object, so the object
 * will not go away until the handle is deleted.
 */
int
drm_gem_open_ioctl(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	struct drm_gem_open *args = data;
	struct drm_gem_object *obj;
	int ret;
	u32 handle;

	if (!(dev->driver->driver_features & DRIVER_GEM))
		return -ENODEV;

	spin_lock(&dev->object_name_lock);
	obj = idr_find(&dev->object_name_idr, (int) args->name);
	if (obj)
		drm_gem_object_reference(obj);
	spin_unlock(&dev->object_name_lock);
	if (!obj)
		return -ENOENT;

	ret = drm_gem_handle_create(file_priv, obj, &handle);
	drm_gem_object_unreference_unlocked(obj);
	if (ret)
		return ret;

	args->handle = handle;
	args->size = obj->size;

	return 0;
}

/**
 * Called at device open time, sets up the structure for handling refcounting
 * of mm objects.
 */
void
drm_gem_open(struct drm_device *dev, struct drm_file *file_private)
{
	idr_init(&file_private->object_idr);
	spin_lock_init(&file_private->table_lock);
}

/**
 * Called at device close to release the file's
 * handle references on objects.
 */
static int
drm_gem_object_release_handle(int id, void *ptr, void *data)
{
	struct drm_file *file_priv = data;
	struct drm_gem_object *obj = ptr;
	struct drm_device *dev = obj->dev;

#if defined(FREEBSD_NOTYET)
	drm_gem_remove_prime_handles(obj, file_priv);
#endif

	if (dev->driver->gem_close_object)
		dev->driver->gem_close_object(obj, file_priv);

	drm_gem_object_handle_unreference_unlocked(obj);

	return 0;
}

/**
 * Called at close time when the filp is going away.
 *
 * Releases any remaining references on objects by this filp.
 */
void
drm_gem_release(struct drm_device *dev, struct drm_file *file_private)
{
	idr_for_each(&file_private->object_idr,
		     &drm_gem_object_release_handle, file_private);
	idr_destroy(&file_private->object_idr);
}

void
drm_gem_object_release(struct drm_gem_object *obj)
{
#ifdef FREEBSD_NOTYET
	if (obj->filp)
	    fput(obj->filp);
#else
	/*
	 * obj->vm_obj can be NULL for private gem objects.
	 */
	vm_object_deallocate(obj->vm_obj);
#endif
}
EXPORT_SYMBOL(drm_gem_object_release);

/**
 * Called after the last reference to the object has been lost.
 * Must be called holding struct_ mutex
 *
 * Frees the object
 */
void
drm_gem_object_free(struct kref *kref)
{
	struct drm_gem_object *obj = (struct drm_gem_object *) kref;
	struct drm_device *dev = obj->dev;

	BUG_ON(!mutex_is_locked(&dev->struct_mutex));

	if (dev->driver->gem_free_object != NULL)
		dev->driver->gem_free_object(obj);
}
EXPORT_SYMBOL(drm_gem_object_free);

static void drm_gem_object_ref_bug(struct kref *list_kref)
{
	BUG();
}

/**
 * Called after the last handle to the object has been closed
 *
 * Removes any name for the object. Note that this must be
 * called before drm_gem_object_free or we'll be touching
 * freed memory
 */
void drm_gem_object_handle_free(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;

	/* Remove any name for this object */
	spin_lock(&dev->object_name_lock);
	if (obj->name) {
		idr_remove(&dev->object_name_idr, obj->name);
		obj->name = 0;
		spin_unlock(&dev->object_name_lock);
		/*
		 * The object name held a reference to this object, drop
		 * that now.
		*
		* This cannot be the last reference, since the handle holds one too.
		 */
		kref_put(&obj->refcount, drm_gem_object_ref_bug);
	} else
		spin_unlock(&dev->object_name_lock);

}
EXPORT_SYMBOL(drm_gem_object_handle_free);

#ifdef __linux__

void drm_gem_vm_open(struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = vma->vm_private_data;

	drm_gem_object_reference(obj);

	mutex_lock(&obj->dev->struct_mutex);
	drm_vm_open_locked(obj->dev, vma);
	mutex_unlock(&obj->dev->struct_mutex);
}
EXPORT_SYMBOL(drm_gem_vm_open);

void drm_gem_vm_close(struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	struct drm_device *dev = obj->dev;

	mutex_lock(&dev->struct_mutex);
	drm_vm_close_locked(obj->dev, vma);
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);
}
EXPORT_SYMBOL(drm_gem_vm_close);


/**
 * drm_gem_mmap - memory map routine for GEM objects
 * @filp: DRM file pointer
 * @vma: VMA for the area to be mapped
 *
 * If a driver supports GEM object mapping, mmap calls on the DRM file
 * descriptor will end up here.
 *
 * If we find the object based on the offset passed in (vma->vm_pgoff will
 * contain the fake offset we created when the GTT map ioctl was called on
 * the object), we set up the driver fault handler so that any accesses
 * to the object can be trapped, to perform migration, GTT binding, surface
 * register allocation, or performance monitoring.
 */
int drm_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct drm_gem_mm *mm = dev->mm_private;
	struct drm_local_map *map = NULL;
	struct drm_gem_object *obj;
	struct drm_hash_item *hash;
	int ret = 0;

	if (drm_device_is_unplugged(dev))
		return -ENODEV;

	mutex_lock(&dev->struct_mutex);

	if (drm_ht_find_item(&mm->offset_hash, vma->vm_pgoff, &hash)) {
		mutex_unlock(&dev->struct_mutex);
		return drm_mmap(filp, vma);
	}

	map = drm_hash_entry(hash, struct drm_map_list, hash)->map;
	if (!map ||
	    ((map->flags & _DRM_RESTRICTED) && !capable(CAP_SYS_ADMIN))) {
		ret =  -EPERM;
		goto out_unlock;
	}

	/* Check for valid size. */
	if (map->size < vma->vm_end - vma->vm_start) {
		ret = -EINVAL;
		goto out_unlock;
	}

	obj = map->handle;
	if (!obj->dev->driver->gem_vm_ops) {
		ret = -EINVAL;
		goto out_unlock;
	}

	vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_ops = obj->dev->driver->gem_vm_ops;
	vma->vm_private_data = map->handle;
	vma->vm_page_prot =  pgprot_writecombine(vm_get_page_prot(vma->vm_flags));

	/* Take a ref for this mapping of the object, so that the fault
	 * handler can dereference the mmap offset's pointer to the object.
	 * This reference is cleaned up by the corresponding vm_close
	 * (which should happen whether the vma was created by this call, or
	 * by a vm_open due to mremap or partial unmap or whatever).
	 */
	drm_gem_object_reference(obj);

	drm_vm_open_locked(dev, vma);

out_unlock:
	mutex_unlock(&dev->struct_mutex);

	return ret;
}
EXPORT_SYMBOL(drm_gem_mmap);
#endif /* __linux __ */

static struct drm_gem_object *
drm_gem_object_from_offset(struct drm_device *dev, vm_ooffset_t offset)
{
	struct drm_gem_object *obj;
	struct drm_gem_mm *mm;
	struct drm_hash_item *map_list;

	if ((offset & DRM_GEM_MAPPING_MASK) != DRM_GEM_MAPPING_KEY)
		return (NULL);
	offset &= ~DRM_GEM_MAPPING_KEY;
	mm = dev->mm_private;
	if (drm_ht_find_item(&mm->offset_hash, DRM_GEM_MAPPING_IDX(offset),
	    &map_list) != 0) {
	DRM_DEBUG("drm_gem_object_from_offset: offset 0x%jx obj not found\n",
		    (uintmax_t)offset);
		return (NULL);
	}
	obj = __containerof(map_list, struct drm_gem_object, map_list);
	return (obj);
}

int
drm_gem_mmap_single(struct drm_device *dev, vm_ooffset_t *offset, vm_size_t size,
    struct vm_object **obj_res, int nprot)
{
	struct drm_gem_object *gem_obj;
	struct vm_object *vm_obj;

	mutex_lock(&dev->struct_mutex);
	gem_obj = drm_gem_object_from_offset(dev, *offset);
	if (gem_obj == NULL) {
		mutex_unlock(&dev->struct_mutex);
		return (-ENODEV);
	}
	drm_gem_object_reference(gem_obj);
	mutex_unlock(&dev->struct_mutex);
	vm_obj = cdev_pager_allocate(gem_obj, OBJT_MGTDEVICE,
	    dev->driver->gem_pager_ops, size, nprot,
	    DRM_GEM_MAPPING_MAPOFF(*offset), curthread->td_ucred);
	if (vm_obj == NULL) {
		drm_gem_object_unreference_unlocked(gem_obj);
		return (-EINVAL);
	}
	*offset = DRM_GEM_MAPPING_MAPOFF(*offset);
	*obj_res = vm_obj;
	return (0);
}

void
drm_gem_pager_dtr(void *handle)
{
	struct drm_gem_object *obj;
	struct drm_device *dev;

	obj = handle;
	dev = obj->dev;

	mutex_lock(&dev->struct_mutex);
	drm_gem_free_mmap_offset(obj);
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);
}
