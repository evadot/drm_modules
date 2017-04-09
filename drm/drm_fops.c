/**
 * \file drm_fops.c
 * File operations for DRM
 *
 * \author Rickard E. (Rik) Faith <faith@valinux.com>
 * \author Daryll Strauss <daryll@valinux.com>
 * \author Gareth Hughes <gareth@valinux.com>
 */

/*
 * Created: Mon Jan  4 08:58:31 1999 by faith@valinux.com
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

#include <drm/drmP.h>
#ifdef __linux__
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/module.h>

/* from BKL pushdown: note that nothing else serializes idr_find() */
DEFINE_MUTEX(drm_global_mutex);
EXPORT_SYMBOL(drm_global_mutex);
#endif

static int drm_open_helper(struct cdev *kdev, int flags, int fmt,
			   DRM_STRUCTPROC *p, struct drm_device *dev);

static int drm_setup(struct drm_device * dev)
{
	int i;
	int ret;

	if (dev->driver->firstopen) {
		ret = dev->driver->firstopen(dev);
		if (ret != 0)
			return ret;
	}

	atomic_set(&dev->ioctl_count, 0);
	atomic_set(&dev->vma_count, 0);

	if (drm_core_check_feature(dev, DRIVER_HAVE_DMA) &&
	    !drm_core_check_feature(dev, DRIVER_MODESET)) {
		dev->buf_use = 0;
		atomic_set(&dev->buf_alloc, 0);

		i = drm_dma_setup(dev);
		if (i < 0)
			return i;
	}

	/*
	 * FIXME Linux<->FreeBSD: counter incremented in drm_open() and
	 * reset to 0 here.
	 */
#if 0
	for (i = 0; i < ARRAY_SIZE(dev->counts); i++)
		atomic_set(&dev->counts[i], 0);
#endif

	dev->sigdata.lock = NULL;

	dev->context_flag = 0;
	dev->interrupt_flag = 0;
	dev->dma_flag = 0;
	dev->last_context = 0;
	dev->last_switch = 0;
	dev->last_checked = 0;
#ifdef FREEBSD_NOTYET
	init_waitqueue_head(&dev->context_wait);
#else
	DRM_INIT_WAITQUEUE(&dev->context_wait);
#endif
	dev->if_version = 0;

#ifdef FREEBSD_NOTYET
	dev->ctx_start = 0;
	dev->lck_start = 0;

	dev->buf_async = NULL;
	init_waitqueue_head(&dev->buf_readers);
	init_waitqueue_head(&dev->buf_writers);
#endif /* FREEBSD_NOTYET */

	DRM_DEBUG("\n");

	/*
	 * The kernel's context could be created here, but is now created
	 * in drm_dma_enqueue.  This is more resource-efficient for
	 * hardware that does not do DMA, but may mean that
	 * drm_select_queue fails between the time the interrupt is
	 * initialized and the time the queues are initialized.
	 */

	return 0;
}

/**
 * Open file.
 *
 * \param inode device inode
 * \param filp file pointer.
 * \return zero on success or a negative number on failure.
 *
 * Searches the DRM device with the same minor number, calls open_helper(), and
 * increments the device open count. If the open count was previous at zero,
 * i.e., it's the first that the device is open, then calls setup().
 */
int drm_open(struct cdev *kdev, int flags, int fmt, DRM_STRUCTPROC *p)
{
	struct drm_device *dev = NULL;
	struct drm_minor *minor;
	int retcode = 0;
	int need_setup = 0;

	minor = kdev->si_drv1;
	if (!minor)
		return ENODEV;

	if (!(dev = minor->dev))
		return ENODEV;

#ifdef __linux__
	if (drm_device_is_unplugged(dev))
		return -ENODEV;
#endif

	/*
	 * FIXME Linux<->FreeBSD: On Linux, counter updated outside
	 * global mutex.
	 */
	if (!dev->open_count++)
		need_setup = 1;
	sx_xlock(&drm_global_mutex);
#ifdef __linux__
	mutex_lock(&dev->struct_mutex);
	old_imapping = inode->i_mapping;
	old_mapping = dev->dev_mapping;
	if (old_mapping == NULL)
		dev->dev_mapping = &inode->i_data;
	/* ihold ensures nobody can remove inode with our i_data */
	ihold(container_of(dev->dev_mapping, struct inode, i_data));
	inode->i_mapping = dev->dev_mapping;
	filp->f_mapping = dev->dev_mapping;
	mutex_unlock(&dev->struct_mutex);
#endif

#ifdef __linux__
	retcode = drm_open_helper(inode, filp, dev);
	if (retcode)
		goto err_undo;
#elif __FreeBSD__
	retcode = drm_open_helper(kdev, flags, fmt, p, dev);
	if (retcode) {
		sx_xunlock(&drm_global_mutex);
		return (-retcode);
	}
#endif
	atomic_inc(&dev->counts[_DRM_STAT_OPENS]);
	if (need_setup) {
		retcode = drm_setup(dev);
		if (retcode)
			goto err_undo;
	}
	sx_xunlock(&drm_global_mutex);
	return 0;

err_undo:
#ifdef __linux__
	mutex_lock(&dev->struct_mutex);
	filp->f_mapping = old_imapping;
	inode->i_mapping = old_imapping;
	iput(container_of(dev->dev_mapping, struct inode, i_data));
	dev->dev_mapping = old_mapping;
	mutex_unlock(&dev->struct_mutex);
	dev->open_count--;
	return retcode;
#elif __FreeBSD__
	mtx_lock(&Giant); /* FIXME: Giant required? */
	device_unbusy(dev->dev);
	mtx_unlock(&Giant);
	dev->open_count--;
	sx_xunlock(&drm_global_mutex);
	return -retcode;
#endif
}
EXPORT_SYMBOL(drm_open);

#ifdef __linux__
/**
 * File \c open operation.
 *
 * \param inode device inode.
 * \param filp file pointer.
 *
 * Puts the dev->fops corresponding to the device minor number into
 * \p filp, call the \c open method, and restore the file operations.
 */
int drm_stub_open(struct inode *inode, struct file *filp)
{
	struct drm_device *dev = NULL;
	struct drm_minor *minor;
	int minor_id = iminor(inode);
	int err = -ENODEV;
	const struct file_operations *old_fops;

	DRM_DEBUG("\n");

	mutex_lock(&drm_global_mutex);
	minor = idr_find(&drm_minors_idr, minor_id);
	if (!minor)
		goto out;

	if (!(dev = minor->dev))
		goto out;

	if (drm_device_is_unplugged(dev))
		goto out;

	old_fops = filp->f_op;
	filp->f_op = fops_get(dev->driver->fops);
	if (filp->f_op == NULL) {
		filp->f_op = old_fops;
		goto out;
	}
	if (filp->f_op->open && (err = filp->f_op->open(inode, filp))) {
		fops_put(filp->f_op);
		filp->f_op = fops_get(old_fops);
	}
	fops_put(old_fops);

out:
	mutex_unlock(&drm_global_mutex);
	return err;
}

/**
 * Check whether DRI will run on this CPU.
 *
 * \return non-zero if the DRI will run on this CPU, or zero otherwise.
 */
static int drm_cpu_valid(void)
{
#if defined(__i386__)
	if (boot_cpu_data.x86 == 3)
		return 0;	/* No cmpxchg on a 386 */
#endif
#if defined(__sparc__) && !defined(__sparc_v9__)
	return 0;		/* No cmpxchg before v9 sparc. */
#endif
	return 1;
}
#endif /* __linux__ */

/**
 * Called whenever a process opens /dev/drm.
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param dev device.
 * \return zero on success or a negative number on failure.
 *
 * Creates and initializes a drm_file structure for the file private data in \p
 * filp and add it into the double linked list in \p dev.
 */
static int drm_open_helper(struct cdev *kdev, int flags, int fmt,
			   DRM_STRUCTPROC *p, struct drm_device *dev)
{
	struct drm_file *priv;
	int ret;

	if (flags & O_EXCL)
		return -EBUSY;	/* No exclusive opens */
	if (dev->switch_power_state != DRM_SWITCH_POWER_ON)
		return -EINVAL;

	DRM_DEBUG("pid = %d, device = %s\n", DRM_CURRENTPID, devtoname(kdev));

#ifdef FREEBSD_NOTYET
	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
#else
	priv = malloc(sizeof(*priv), DRM_MEM_FILES, M_NOWAIT | M_ZERO);
#endif
	if (!priv)
		return -ENOMEM;

#ifdef __linux__
	filp->private_data = priv;
	priv->filp = filp;
	priv->uid = current_euid();
	priv->pid = get_pid(task_pid(current));
	priv->minor = idr_find(&drm_minors_idr, minor_id);
	priv->ioctl_count = 0;
	/* for compatibility root is always authenticated */
	priv->authenticated = capable(CAP_SYS_ADMIN);
#elif __FreeBSD__
	priv->uid = p->td_ucred->cr_svuid;
	priv->pid = p->td_proc->p_pid;
	priv->minor = kdev->si_drv1;
	priv->ioctl_count = 0;
	/* for compatibility root is always authenticated */
	priv->authenticated = DRM_SUSER(p);
#endif
	priv->lock_count = 0;

	INIT_LIST_HEAD(&priv->lhead);
	INIT_LIST_HEAD(&priv->fbs);
	INIT_LIST_HEAD(&priv->event_list);
#ifdef __linux__
	init_waitqueue_head(&priv->event_wait);
#endif
	priv->event_space = 4096; /* set aside 4k for event buffer */

	if (dev->driver->driver_features & DRIVER_GEM)
		drm_gem_open(dev, priv);

#ifdef FREEBSD_NOTYET
	if (drm_core_check_feature(dev, DRIVER_PRIME))
		drm_prime_init_file_private(&priv->prime);
#endif /* FREEBSD_NOTYET */

	if (dev->driver->open) {
		ret = dev->driver->open(dev, priv);
		if (ret < 0)
			goto out_free;
	}


	/* if there is no current master make this fd it */
#ifdef FREEBSD_NOTYET
	mutex_lock(&dev->struct_mutex);
#else
	DRM_LOCK(dev);
#endif
	if (!priv->minor->master) {
		/* create a new master */
		priv->minor->master = drm_master_create(priv->minor);
		if (!priv->minor->master) {
#ifdef FREEBSD_NOTYET
			mutex_unlock(&dev->struct_mutex);
#else
			DRM_UNLOCK(dev);
#endif
			ret = -ENOMEM;
			goto out_free;
		}

		priv->is_master = 1;
		/* take another reference for the copy in the local file priv */
		priv->master = drm_master_get(priv->minor->master);

		priv->authenticated = 1;

#ifdef FREEBSD_NOTYET
		mutex_unlock(&dev->struct_mutex);
#else
		DRM_UNLOCK(dev);
#endif
		if (dev->driver->master_create) {
			ret = dev->driver->master_create(dev, priv->master);
			if (ret) {
#ifdef FREEBSD_NOTYET
				mutex_lock(&dev->struct_mutex);
#else
				DRM_LOCK(dev);
#endif
				/* drop both references if this fails */
				drm_master_put(&priv->minor->master);
				drm_master_put(&priv->master);
#ifdef FREEBSD_NOTYET
				mutex_unlock(&dev->struct_mutex);
#else
				DRM_UNLOCK(dev);
#endif
				goto out_free;
			}
		}
#ifdef FREEBSD_NOTYET
		mutex_lock(&dev->struct_mutex);
#else
		DRM_LOCK(dev);
#endif
		if (dev->driver->master_set) {
			ret = dev->driver->master_set(dev, priv, true);
			if (ret) {
				/* drop both references if this fails */
				drm_master_put(&priv->minor->master);
				drm_master_put(&priv->master);
#ifdef FREEBSD_NOTYET
				mutex_unlock(&dev->struct_mutex);
#else
				DRM_UNLOCK(dev);
#endif
				goto out_free;
			}
		}
#ifdef FREEBSD_NOTYET
		mutex_unlock(&dev->struct_mutex);
#else
		DRM_UNLOCK(dev);
#endif
	} else {
		/* get a reference to the master */
		priv->master = drm_master_get(priv->minor->master);
#ifdef FREEBSD_NOTYET
		mutex_unlock(&dev->struct_mutex);
#else
		DRM_UNLOCK(dev);
#endif
	}

#ifdef FREEBSD_NOTYET
	mutex_lock(&dev->struct_mutex);
#else
	DRM_LOCK(dev);
#endif
	list_add(&priv->lhead, &dev->filelist);
#ifdef FREEBSD_NOTYET
	mutex_unlock(&dev->struct_mutex);
#else
	DRM_UNLOCK(dev);
#endif

#ifdef __linux__
#ifdef __alpha__
	/*
	 * Default the hose
	 */
	if (!dev->hose) {
		struct pci_dev *pci_dev;
		pci_dev = pci_get_class(PCI_CLASS_DISPLAY_VGA << 8, NULL);
		if (pci_dev) {
			dev->hose = pci_dev->sysdata;
			pci_dev_put(pci_dev);
		}
		if (!dev->hose) {
			struct pci_bus *b = pci_bus_b(pci_root_buses.next);
			if (b)
				dev->hose = b->sysdata;
		}
	}
#endif

	return 0;
#elif  __FreeBSD__
	mtx_lock(&Giant); /* FIXME: Giant required? */
	device_busy(dev->dev);
	mtx_unlock(&Giant);

	ret = devfs_set_cdevpriv(priv, drm_release);
	if (ret != 0)
		drm_release(priv);

	return ret;
#endif

      out_free:
#ifdef FREEBSD_NOTYET
	kfree(priv);
	filp->private_data = NULL;
#else
	free(priv, DRM_MEM_FILES);
#endif
	return ret;
}

#ifdef __linux__
/** No-op. */
int drm_fasync(int fd, struct file *filp, int on)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;

	DRM_DEBUG("fd = %d, device = 0x%lx\n", fd,
		  (long)old_encode_dev(priv->minor->device));
	return fasync_helper(fd, filp, on, &dev->buf_async);
}
EXPORT_SYMBOL(drm_fasync);
#endif

static void drm_master_release(struct drm_device *dev, struct drm_file *file_priv)
{

	if (drm_i_have_hw_lock(dev, file_priv)) {
		DRM_DEBUG("File %p released, freeing lock for context %d\n",
			  file_priv, _DRM_LOCKING_CONTEXT(file_priv->master->lock.hw_lock->lock));
		drm_lock_free(&file_priv->master->lock,
			      _DRM_LOCKING_CONTEXT(file_priv->master->lock.hw_lock->lock));
	}
}

static void drm_events_release(struct drm_file *file_priv)
{
	struct drm_device *dev = file_priv->minor->dev;
	struct drm_pending_event *e, *et;
	struct drm_pending_vblank_event *v, *vt;
	unsigned long flags;

#ifdef FREEBSD_NOTYET
	spin_lock_irqsave(&dev->event_lock, flags);
#else
	DRM_SPINLOCK_IRQSAVE(&dev->event_lock, flags);
#endif

	/* Remove pending flips */
	list_for_each_entry_safe(v, vt, &dev->vblank_event_list, base.link)
		if (v->base.file_priv == file_priv) {
			list_del(&v->base.link);
			drm_vblank_put(dev, v->pipe);
			v->base.destroy(&v->base);
		}

	/* Remove unconsumed events */
	list_for_each_entry_safe(e, et, &file_priv->event_list, link)
		e->destroy(e);

#ifdef FREEBSD_NOTYET
	spin_unlock_irqrestore(&dev->event_lock, flags);
#else
	DRM_SPINUNLOCK_IRQRESTORE(&dev->event_lock, flags);
#endif
}

/**
 * Release file.
 *
 * \param inode device inode
 * \param file_priv DRM file private.
 * \return zero on success or a negative number on failure.
 *
 * If the hardware lock is held then free it, and take it again for the kernel
 * context since it's necessary to reclaim buffers. Unlink the file private
 * data from its list and free it. Decreases the open count and if it reaches
 * zero calls drm_lastclose().
 */
void drm_release(void *data)
{
	struct drm_file *file_priv = data;
	struct drm_device *dev = file_priv->minor->dev;

#ifdef FREEBSD_NOTYET
	mutex_lock(&drm_global_mutex);
#else
	sx_xlock(&drm_global_mutex);
#endif

	DRM_DEBUG("open_count = %d\n", dev->open_count);

	if (dev->driver->preclose)
		dev->driver->preclose(dev, file_priv);

	/* ========================================================
	 * Begin inline drm_release
	 */

	DRM_DEBUG("pid = %d, device = 0x%lx, open_count = %d\n",
		  DRM_CURRENTPID,
		  (long)file_priv->minor->device,
		  dev->open_count);

	/* Release any auth tokens that might point to this file_priv,
	   (do that under the drm_global_mutex) */
	if (file_priv->magic)
		(void) drm_remove_magic(file_priv->master, file_priv->magic);

	/* if the master has gone away we can't do anything with the lock */
	if (file_priv->minor->master)
		drm_master_release(dev, file_priv);

	if (drm_core_check_feature(dev, DRIVER_HAVE_DMA))
		drm_core_reclaim_buffers(dev, file_priv);

	drm_events_release(file_priv);

#ifdef __FreeBSD__
	seldrain(&file_priv->event_poll);
#endif

	if (dev->driver->driver_features & DRIVER_MODESET)
		drm_fb_release(file_priv);

	if (dev->driver->driver_features & DRIVER_GEM)
		drm_gem_release(dev, file_priv);

#ifdef FREEBSD_NOTYET
	mutex_lock(&dev->ctxlist_mutex);
	if (!list_empty(&dev->ctxlist)) {
		struct drm_ctx_list *pos, *n;

		list_for_each_entry_safe(pos, n, &dev->ctxlist, head) {
			if (pos->tag == file_priv &&
			    pos->handle != DRM_KERNEL_CONTEXT) {
				if (dev->driver->context_dtor)
					dev->driver->context_dtor(dev,
								  pos->handle);

				drm_ctxbitmap_free(dev, pos->handle);

				list_del(&pos->head);
				kfree(pos);
				--dev->ctx_count;
			}
		}
	}
	mutex_unlock(&dev->ctxlist_mutex);
#endif /* FREEBSD_NOTYET */

#ifdef FREEBSD_NOTYET
	mutex_lock(&dev->struct_mutex);
#else
	DRM_LOCK(dev);
#endif

	if (file_priv->is_master) {
		struct drm_master *master = file_priv->master;
		struct drm_file *temp;
		list_for_each_entry(temp, &dev->filelist, lhead) {
			if ((temp->master == file_priv->master) &&
			    (temp != file_priv))
				temp->authenticated = 0;
		}

		/**
		 * Since the master is disappearing, so is the
		 * possibility to lock.
		 */

		if (master->lock.hw_lock) {
			if (dev->sigdata.lock == master->lock.hw_lock)
				dev->sigdata.lock = NULL;
			master->lock.hw_lock = NULL;
			master->lock.file_priv = NULL;
			DRM_WAKEUP_INT(&master->lock.lock_queue);
		}

		if (file_priv->minor->master == file_priv->master) {
			/* drop the reference held my the minor */
			if (dev->driver->master_drop)
				dev->driver->master_drop(dev, file_priv, true);
			drm_master_put(&file_priv->minor->master);
		}
	}

	/* drop the reference held my the file priv */
	drm_master_put(&file_priv->master);
	file_priv->is_master = 0;
	list_del(&file_priv->lhead);
#ifdef FREEBSD_NOTYET
	mutex_unlock(&dev->struct_mutex);
#else
	DRM_UNLOCK(dev);
#endif

	if (dev->driver->postclose)
		dev->driver->postclose(dev, file_priv);

#ifdef FREEBSD_NOTYET
	if (drm_core_check_feature(dev, DRIVER_PRIME))
		drm_prime_destroy_file_private(&file_priv->prime);
#endif /* FREEBSD_NOTYET */

#ifdef __linux__
	put_pid(file_priv->pid);
#endif
#ifdef FREEBSD_NOTYET
	kfree(file_priv);
#else
	free(file_priv, DRM_MEM_FILES);
#endif

	/* ========================================================
	 * End inline drm_release
	 */

	atomic_inc(&dev->counts[_DRM_STAT_CLOSES]);
#ifdef __FreeBSD__
	mtx_lock(&Giant);
	device_unbusy(dev->dev);
	mtx_unlock(&Giant);
#endif
	if (!--dev->open_count) {
		if (atomic_read(&dev->ioctl_count)) {
			DRM_ERROR("Device busy: %d\n",
				  atomic_read(&dev->ioctl_count));
		} else
			drm_lastclose(dev);
	}
#ifdef FREEBSD_NOTYET
	mutex_unlock(&drm_global_mutex);
#else
	sx_xunlock(&drm_global_mutex);
#endif
}
EXPORT_SYMBOL(drm_release);

static bool
drm_dequeue_event(struct drm_file *file_priv, struct uio *uio,
    struct drm_pending_event **out)
{
	struct drm_pending_event *e;
	bool ret = false;

	/* Already locked in drm_read(). */
	/* spin_lock_irqsave(&dev->event_lock, flags); */

	*out = NULL;
	if (list_empty(&file_priv->event_list))
		goto out;
	e = list_first_entry(&file_priv->event_list,
			     struct drm_pending_event, link);
	if (e->event->length > uio->uio_resid)
		goto out;

	file_priv->event_space += e->event->length;
	list_del(&e->link);
	*out = e;
	ret = true;

out:
	/* spin_unlock_irqrestore(&dev->event_lock, flags); */
	return ret;
}

int
drm_read(struct cdev *kdev, struct uio *uio, int ioflag)
{
	struct drm_file *file_priv;
	struct drm_device *dev;
	struct drm_pending_event *e;
	ssize_t error;

	error = devfs_get_cdevpriv((void **)&file_priv);
	if (error != 0) {
		DRM_ERROR("can't find authenticator\n");
		return (EINVAL);
	}

	dev = drm_get_device_from_kdev(kdev);
	mtx_lock(&dev->event_lock);
	while (list_empty(&file_priv->event_list)) {
		if ((ioflag & O_NONBLOCK) != 0) {
			error = EAGAIN;
			goto out;
		}
		error = msleep(&file_priv->event_space, &dev->event_lock,
	           PCATCH, "drmrea", 0);
	       if (error != 0)
		       goto out;
	}

	while (drm_dequeue_event(file_priv, uio, &e)) {
		mtx_unlock(&dev->event_lock);
		error = uiomove(e->event, e->event->length, uio);
		CTR3(KTR_DRM, "drm_event_dequeued %d %d %d", curproc->p_pid,
		    e->event->type, e->event->length);

		e->destroy(e);
		if (error != 0)
			return (error);
		mtx_lock(&dev->event_lock);
	}

out:
	mtx_unlock(&dev->event_lock);
	return (error);
}
EXPORT_SYMBOL(drm_read);

int
drm_poll(struct cdev *kdev, int events, struct thread *td)
{
	struct drm_file *file_priv;
	struct drm_device *dev;
	int error, revents;

	error = devfs_get_cdevpriv((void **)&file_priv);
	if (error != 0) {
		DRM_ERROR("can't find authenticator\n");
		return (EINVAL);
	}

	dev = drm_get_device_from_kdev(kdev);

	revents = 0;
	mtx_lock(&dev->event_lock);
	if ((events & (POLLIN | POLLRDNORM)) != 0) {
		if (list_empty(&file_priv->event_list)) {
			CTR0(KTR_DRM, "drm_poll empty list");
			selrecord(td, &file_priv->event_poll);
		} else {
			revents |= events & (POLLIN | POLLRDNORM);
			CTR1(KTR_DRM, "drm_poll revents %x", revents);
		}
	}
	mtx_unlock(&dev->event_lock);
	return (revents);
}
EXPORT_SYMBOL(drm_poll);

#ifdef __FreeBSD__
int
drm_mmap_single(struct cdev *kdev, vm_ooffset_t *offset, vm_size_t size,
    struct vm_object **obj_res, int nprot)
{
	struct drm_device *dev;

	dev = drm_get_device_from_kdev(kdev);
	if (dev->drm_ttm_bdev != NULL) {
		return (-ttm_bo_mmap_single(dev->drm_ttm_bdev, offset, size,
		    obj_res, nprot));
	} else if ((dev->driver->driver_features & DRIVER_GEM) != 0) {
		return (-drm_gem_mmap_single(dev, offset, size, obj_res, nprot));
	} else {
		return (ENODEV);
	}
}

void
drm_event_wakeup(struct drm_pending_event *e)
{
	struct drm_file *file_priv;
	struct drm_device *dev;

	file_priv = e->file_priv;
	dev = file_priv->minor->dev;
	mtx_assert(&dev->event_lock, MA_OWNED);

	wakeup(&file_priv->event_space);
	selwakeup(&file_priv->event_poll);
}
#endif
