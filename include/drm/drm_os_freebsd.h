/**
 * \file drm_os_freebsd.h
 * OS abstraction macros.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef _DRM_OS_FREEBSD_H_
#define	_DRM_OS_FREEBSD_H_

#include <sys/fbio.h>
#include <sys/smp.h>

#define	wait_queue_head_t atomic_t

#define	DRM_IRQ_ARGS		void *arg
typedef void			irqreturn_t;
#define	IRQ_HANDLED		/* nothing */
#define	IRQ_NONE		/* nothing */

#define	BUILD_BUG_ON_NOT_POWER_OF_2(x)

#define	KHZ2PICOS(a)	(1000000000UL/(a))

#define	HZ			hz
#define	DRM_HZ			hz
#define	DRM_CURRENTPID		curthread->td_proc->p_pid
#define	DRM_SUSER(p)		(priv_check(p, PRIV_DRIVER) == 0)
#define	udelay(usecs)		DELAY(usecs)
#define	mdelay(msecs)		do { int loops = (msecs);		\
				  while (loops--) DELAY(1000);		\
				} while (0)
#define	DRM_UDELAY(udelay)	DELAY(udelay)
#define	drm_msleep(x, msg)	pause((msg), ((int64_t)(x)) * hz / 1000)
#define	DRM_MSLEEP(msecs)	drm_msleep((msecs), "drm_msleep")

#define ioread8(addr)		*(volatile uint8_t *)((char *)addr)
#define ioread16(addr)		*(volatile uint16_t *)((char *)addr)
#define ioread32(addr)		*(volatile uint32_t *)((char *)addr)

#define iowrite8(data, addr)	*(volatile uint8_t *)((char *)addr) = data;
#define iowrite16(data, addr)	*(volatile uint16_t *)((char *)addr) = data;
#define iowrite32(data, addr)	*(volatile uint32_t *)((char *)addr) = data;

#define	DRM_READ8(map, offset)						\
	*(volatile u_int8_t *)(((vm_offset_t)(map)->handle) +		\
	    (vm_offset_t)(offset))
#define	DRM_READ16(map, offset)						\
	le16toh(*(volatile u_int16_t *)(((vm_offset_t)(map)->handle) +	\
	    (vm_offset_t)(offset)))
#define	DRM_READ32(map, offset)						\
	le32toh(*(volatile u_int32_t *)(((vm_offset_t)(map)->handle) +	\
	    (vm_offset_t)(offset)))
#define	DRM_READ64(map, offset)						\
	le64toh(*(volatile u_int64_t *)(((vm_offset_t)(map)->handle) +	\
	    (vm_offset_t)(offset)))
#define	DRM_WRITE8(map, offset, val)					\
	*(volatile u_int8_t *)(((vm_offset_t)(map)->handle) +		\
	    (vm_offset_t)(offset)) = val
#define	DRM_WRITE16(map, offset, val)					\
	*(volatile u_int16_t *)(((vm_offset_t)(map)->handle) +		\
	    (vm_offset_t)(offset)) = htole16(val)
#define	DRM_WRITE32(map, offset, val)					\
	*(volatile u_int32_t *)(((vm_offset_t)(map)->handle) +		\
	    (vm_offset_t)(offset)) = htole32(val)
#define	DRM_WRITE64(map, offset, val)					\
	*(volatile u_int64_t *)(((vm_offset_t)(map)->handle) +		\
	    (vm_offset_t)(offset)) = htole64(val)

#define	smp_rmb()			rmb()
#define	smp_wmb()			wmb()
#define	smp_mb__before_atomic_inc()	mb()
#define	smp_mb__after_atomic_inc()	mb()

#define	do_div(a, b)		((a) /= (b))
#define	div64_u64(a, b)		((a) / (b))
#define	lower_32_bits(n)	((u32)(n))
#define	upper_32_bits(n)	((u32)(((n) >> 16) >> 16))

#define min_t(type, x, y) ({			\
	type __min1 = (x);			\
	type __min2 = (y);			\
	__min1 < __min2 ? __min1 : __min2; })

#define max_t(type, x, y) ({			\
	type __max1 = (x);			\
	type __max2 = (y);			\
	__max1 > __max2 ? __max1 : __max2; })

#define	memset_io(a, b, c)	memset((a), (b), (c))
#define	memcpy_fromio(a, b, c)	memcpy((a), (b), (c))
#define	memcpy_toio(a, b, c)	memcpy((a), (b), (c))

/* XXXKIB what is the right code for the FreeBSD ? */
/* kib@ used ENXIO here -- dumbbell@ */
#define	EREMOTEIO	EIO
#define	ERESTARTSYS	512 /* Same value as Linux. */

#define	KTR_DRM		KTR_DEV
#define	KTR_DRM_REG	KTR_SPARE3

#define	DRM_AGP_KERN	struct agp_info
#define	DRM_AGP_MEM	void

#define	PCI_VENDOR_ID_APPLE		0x106b
#define	PCI_VENDOR_ID_ASUSTEK		0x1043
#define	PCI_VENDOR_ID_ATI		0x1002
#define	PCI_VENDOR_ID_DELL		0x1028
#define	PCI_VENDOR_ID_HP		0x103c
#define	PCI_VENDOR_ID_IBM		0x1014
#define	PCI_VENDOR_ID_INTEL		0x8086
#define	PCI_VENDOR_ID_SERVERWORKS	0x1166
#define	PCI_VENDOR_ID_SONY		0x104d
#define	PCI_VENDOR_ID_VIA		0x1106

#define	div_u64(n, d)		((n) / (d))

#define	IS_ALIGNED(x, y)	(((x) & ((y) - 1)) == 0)

#define	page_to_phys(x) VM_PAGE_TO_PHYS(x)

#define	drm_get_device_from_kdev(_kdev)	(((struct drm_minor *)(_kdev)->si_drv1)->dev)

#define DRM_IOC_VOID		IOC_VOID
#define DRM_IOC_READ		IOC_OUT
#define DRM_IOC_WRITE		IOC_IN
#define DRM_IOC_READWRITE	IOC_INOUT
#define DRM_IOC(dir, group, nr, size) _IOC(dir, group, nr, size)

static inline long
__copy_to_user(void __user *to, const void *from, unsigned long n)
{
	return (copyout(from, to, n) != 0 ? n : 0);
}
#define	copy_to_user(to, from, n) __copy_to_user((to), (from), (n))

static inline int
__put_user(size_t size, void *ptr, void *x)
{

	size = copy_to_user(ptr, x, size);

	return (size ? -EFAULT : size);
}
#define	put_user(x, ptr) __put_user(sizeof(*ptr), (ptr), &(x))

static inline unsigned long
__copy_from_user(void *to, const void __user *from, unsigned long n)
{
	return ((copyin(__DECONST(void *, from), to, n) != 0 ? n : 0));
}
#define	copy_from_user(to, from, n) __copy_from_user((to), (from), (n))

static inline int
__get_user(size_t size, const void *ptr, void *x)
{

	size = copy_from_user(x, ptr, size);

	return (size ? -EFAULT : size);
}
#define	get_user(x, ptr) __get_user(sizeof(*ptr), (ptr), &(x))

static inline int
__copy_to_user_inatomic(void __user *to, const void *from, unsigned n)
{

	return (copyout_nofault(from, to, n) != 0 ? n : 0);
}
#define	__copy_to_user_inatomic_nocache(to, from, n) \
    __copy_to_user_inatomic((to), (from), (n))

static inline unsigned long
__copy_from_user_inatomic(void *to, const void __user *from,
    unsigned long n)
{

	/*
	 * XXXKIB.  Equivalent Linux function is implemented using
	 * MOVNTI for aligned moves.  For unaligned head and tail,
	 * normal move is performed.  As such, it is not incorrect, if
	 * only somewhat slower, to use normal copyin.  All uses
	 * except shmem_pwrite_fast() have the destination mapped WC.
	 */
	return ((copyin_nofault(__DECONST(void *, from), to, n) != 0 ? n : 0));
}
#define	__copy_from_user_inatomic_nocache(to, from, n) \
    __copy_from_user_inatomic((to), (from), (n))

static inline int
fault_in_multipages_readable(const char __user *uaddr, int size)
{
	char c;
	int ret = 0;
	const char __user *end = uaddr + size - 1;

	if (unlikely(size == 0))
		return ret;

	while (uaddr <= end) {
		ret = -copyin(uaddr, &c, 1);
		if (ret != 0)
			return -EFAULT;
		uaddr += PAGE_SIZE;
	}

	/* Check whether the range spilled into the next page. */
	if (((unsigned long)uaddr & PAGE_MASK) ==
			((unsigned long)end & PAGE_MASK)) {
		ret = -copyin(end, &c, 1);
	}

	return ret;
}

static inline int
fault_in_multipages_writeable(char __user *uaddr, int size)
{
	int ret = 0;
	char __user *end = uaddr + size - 1;

	if (unlikely(size == 0))
		return ret;

	/*
	 * Writing zeroes into userspace here is OK, because we know that if
	 * the zero gets there, we'll be overwriting it.
	 */
	while (uaddr <= end) {
		ret = subyte(uaddr, 0);
		if (ret != 0)
			return -EFAULT;
		uaddr += PAGE_SIZE;
	}

	/* Check whether the range spilled into the next page. */
	if (((unsigned long)uaddr & PAGE_MASK) ==
			((unsigned long)end & PAGE_MASK))
		ret = subyte(end, 0);

	return ret;
}

enum __drm_capabilities {
	CAP_SYS_ADMIN
};

static inline bool
capable(enum __drm_capabilities cap)
{

	switch (cap) {
	case CAP_SYS_ADMIN:
		return DRM_SUSER(curthread);
	default:
		panic("%s: unhandled capability: %0x", __func__, cap);
		return (false);
	}
}

#define	to_user_ptr(x)		((void *)(uintptr_t)(x))
#define	sigemptyset(set)	SIGEMPTYSET(set)
#define	sigaddset(set, sig)	SIGADDSET(set, sig)

extern unsigned long drm_linux_timer_hz_mask;
#define jiffies			ticks
#define	msecs_to_jiffies(x)	(((int64_t)(x)) * hz / 1000)
#define	timespec_to_jiffies(x)	(((x)->tv_sec * 1000000 + (x)->tv_nsec) * hz / 1000000)
#define	round_jiffies(j)	((unsigned long)(((j) + drm_linux_timer_hz_mask) & ~drm_linux_timer_hz_mask))
#define	round_jiffies_up(j)		round_jiffies(j) /* TODO */
#define	round_jiffies_up_relative(j)	round_jiffies_up(j) /* TODO */

#define	wake_up(queue)				wakeup_one((void *)queue)
#define	wake_up_interruptible(queue)		wakeup_one((void *)queue)
#define	wake_up_all(queue)			wakeup((void *)queue)
#define	wake_up_interruptible_all(queue)	wakeup((void *)queue)

struct _completion_wait {
	spinlock_t lock;
};

struct completion {
	unsigned int done;
	struct _completion_wait wait;
};

#define	INIT_COMPLETION(c) ((c).done = 0);

static inline void
init_completion(struct completion *c)
{

	spin_lock_init(&c->wait.lock);
	c->done = 0;
}

static inline void
free_completion(struct completion *c)
{

	spin_lock_destroy(&c->wait.lock);
}

static inline void
complete_all(struct completion *c)
{

	spin_lock(&c->wait.lock);
	c->done++;
	spin_unlock(&c->wait.lock);
	wakeup(c);
}

static inline long
wait_for_completion_interruptible_timeout(struct completion *c,
    unsigned long timeout)
{
	unsigned long start_jiffies, elapsed_jiffies;
	bool timeout_expired = false, awakened = false;
	long ret = timeout;

	start_jiffies = ticks;

	spin_lock(&c->wait.lock);
	while (c->done == 0 && !timeout_expired) {
		ret = -msleep(c, &c->wait.lock.m, PCATCH, "drmwco", timeout);
		switch(ret) {
		case -EWOULDBLOCK:
			timeout_expired = true;
			ret = 0;
			break;
		case -EINTR:
		case -ERESTART:
			ret = -ERESTARTSYS;
			break;
		case 0:
			awakened = true;
			break;
		}
	}
	spin_unlock(&c->wait.lock);

	if (awakened) {
		elapsed_jiffies = ticks - start_jiffies;
		ret = timeout > elapsed_jiffies ? timeout - elapsed_jiffies : 1;
	}

	return (ret);
}

MALLOC_DECLARE(DRM_MEM_SAREA);
MALLOC_DECLARE(DRM_MEM_DRIVER);
MALLOC_DECLARE(DRM_MEM_FILES);
MALLOC_DECLARE(DRM_MEM_BUFLISTS);
MALLOC_DECLARE(DRM_MEM_CTXBITMAP);
MALLOC_DECLARE(DRM_MEM_SGLISTS);
MALLOC_DECLARE(DRM_MEM_HASHTAB);
MALLOC_DECLARE(DRM_MEM_KMS);

#define	simple_strtol(a, b, c)			strtol((a), (b), (c))

typedef struct drm_pci_id_list
{
	int vendor;
	int device;
	long driver_private;
	char *name;
} drm_pci_id_list_t;

#ifdef __i386__
#define	CONFIG_X86	1
#endif
#ifdef __amd64__
#define	CONFIG_X86	1
#define	CONFIG_X86_64	1
#endif
#ifdef __ia64__
#define	CONFIG_IA64	1
#endif

#if defined(__i386__) || defined(__amd64__)
#define	CONFIG_ACPI
#define	CONFIG_DRM_I915_KMS
#undef	CONFIG_INTEL_IOMMU
#endif

#ifdef COMPAT_FREEBSD32
#define	CONFIG_COMPAT
#endif

#ifndef __arm__
#define	CONFIG_AGP	1
#define	CONFIG_MTRR	1
#endif

#define	CONFIG_FB	1
extern const char *fb_mode_option;

#undef	CONFIG_DEBUG_FS
#undef	CONFIG_VGA_CONSOLE

#define	EXPORT_SYMBOL(x)
#define	EXPORT_SYMBOL_GPL(x)
#define	MODULE_AUTHOR(author)
#define	MODULE_DESCRIPTION(desc)
#define	MODULE_LICENSE(license)
#define	MODULE_PARM_DESC(name, desc)
#define	MODULE_DEVICE_TABLE(name, list)
#define	module_param_named(name, var, type, perm)

#define KBUILD_MODNAME

/* I2C compatibility. */
#define	I2C_M_RD	IIC_M_RD
#define	I2C_M_WR	IIC_M_WR
#define	I2C_M_NOSTART	IIC_M_NOSTART

struct fb_info *	framebuffer_alloc(void);
void			framebuffer_release(struct fb_info *info);

#define	console_lock()
#define	console_unlock()
#define	console_trylock()	true

#define	PM_EVENT_SUSPEND	0x0002
#define	PM_EVENT_QUIESCE	0x0008
#define	PM_EVENT_PRETHAW	PM_EVENT_QUIESCE

static inline int
pci_read_config_byte(device_t kdev, int where, u8 *val)
{

	*val = (u8)pci_read_config(kdev, where, 1);
	return (0);
}

static inline int
pci_write_config_byte(device_t kdev, int where, u8 val)
{

	pci_write_config(kdev, where, val, 1);
	return (0);
}

static inline int
pci_read_config_word(device_t kdev, int where, uint16_t *val)
{

	*val = (uint16_t)pci_read_config(kdev, where, 2);
	return (0);
}

static inline int
pci_write_config_word(device_t kdev, int where, uint16_t val)
{

	pci_write_config(kdev, where, val, 2);
	return (0);
}

static inline int
pci_read_config_dword(device_t kdev, int where, uint32_t *val)
{

	*val = (uint32_t)pci_read_config(kdev, where, 4);
	return (0);
}

static inline int
pci_write_config_dword(device_t kdev, int where, uint32_t val)
{

	pci_write_config(kdev, where, val, 4);
	return (0);
}

static inline void
on_each_cpu(void callback(void *data), void *data, int wait)
{

	smp_rendezvous(NULL, callback, NULL, data);
}

void	hex_dump_to_buffer(const void *buf, size_t len, int rowsize,
	    int groupsize, char *linebuf, size_t linebuflen, bool ascii);

#define KIB_NOTYET()							\
do {									\
	if (drm_debug && drm_notyet)					\
		printf("NOTYET: %s at %s:%d\n", __func__, __FILE__, __LINE__); \
} while (0)

#endif /* _DRM_OS_FREEBSD_H_ */
