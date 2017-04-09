KMOD	= drm

SRCS	= \
	drm_agpsupport.c \
	drm_auth.c \
	drm_bufs.c \
	drm_buffer.c \
	drm_context.c \
	drm_crtc.c \
	drm_crtc_helper.c \
	drm_dma.c \
	drm_dp_helper.c \
	drm_dp_iic_helper.c \
	drm_drv.c \
	drm_edid.c \
	drm_fb_helper.c \
	drm_fops.c \
	drm_gem.c \
	drm_gem_names.c \
	drm_global.c \
	drm_hashtab.c \
	drm_ioc32.c \
	drm_ioctl.c \
	drm_irq.c \
	drm_linux_list_sort.c \
	drm_lock.c \
	drm_memory.c \
	drm_mm.c \
	drm_modes.c \
	drm_pci.c \
	drm_scatter.c \
	drm_stub.c \
	drm_sysctl.c \
	drm_vm.c \
	drm_os_freebsd.c \
	ttm/ttm_agp_backend.c \
	ttm/ttm_lock.c \
	ttm/ttm_object.c \
	ttm/ttm_tt.c \
	ttm/ttm_bo_util.c \
	ttm/ttm_bo.c \
	ttm/ttm_bo_manager.c \
	ttm/ttm_execbuf_util.c \
	ttm/ttm_memory.c \
	ttm/ttm_page_alloc.c \
	ttm/ttm_bo_vm.c \
	ati_pcigart.c

SRCS	+= \
	device_if.h \
	bus_if.h \
	pci_if.h \
	device_if.h \
	iicbus_if.h \
	opt_drm.h \
	opt_vm.h \
	opt_compat.h \
	opt_syscons.h

CFLAGS+=	-I${.CURDIR}/../include

.include <bsd.kmod.mk>
