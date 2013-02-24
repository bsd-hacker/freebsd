#ifndef _MACHINE_EFI_H_
#define _MACHINE_EFI_H_

struct efi_fb {
	int		fb_present;
	uint64_t	fb_addr;
	uint64_t	fb_size;
	int		fb_height;
	int		fb_width;
	int		fb_stride;
	uint32_t	fb_mask_red;
	uint32_t	fb_mask_green;
	uint32_t	fb_mask_blue;
	uint32_t	fb_mask_reserved;
};

struct efi_header {
        size_t          memory_size;
        size_t          descriptor_size;
        uint64_t        descriptor_version;
	struct efi_fb	fb;
};

struct efi_descriptor {
	uint32_t	type;
	vm_offset_t	physical_start;
	vm_offset_t	virtual_start;
	uint64_t	pages;
	uint64_t	attribute;
};

#define efi_next_descriptor(ptr, size) \
	((struct efi_descriptor *)(((uint8_t *) ptr) + size))

#endif  /* _MACHINE_EFI_H_ */
