#include "kernel.h"


uint32_t *os_framebuffer = nullptr;

extern "C" {
    extern virtio_pci_device_t my_gpu;
    extern int virtio_dev_send_command(virtio_pci_device_t *vdev, uint16_t queue_index, void *cmd_buf, uint32_t cmd_len, void *resp_buf, uint32_t resp_len);
    extern uint64_t kernel_virtual_to_physical(void *virtual_addr);
    extern void* kernel_alloc_pages_aligned(uint32_t size, uint32_t alignment);
    extern int virtio_gpu_get_display_info(virtio_pci_device_t *dev, uint32_t *width, uint32_t *height);
}


static virtio_gpu_resource_create_2d cmd_create;
static virtio_gpu_ctrl_hdr           resp_create;

#pragma pack(push, 1)
struct __attribute__((packed, aligned(4))) AttachPacket {
    virtio_gpu_resource_attach_backing attach;
    virtio_gpu_mem_entry entry;
};
struct __attribute__((packed, aligned(4))) strict_scanout_packet {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding_hdr;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t scanout_id;
    uint32_t resource_id;
};
struct __attribute__((packed, aligned(4))) strict_transfer_packet {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding_hdr;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
};
#pragma pack(pop)

static AttachPacket          cmd_attach_packet;
static virtio_gpu_ctrl_hdr   resp_attach;

static strict_scanout_packet cmd_scanout;
static virtio_gpu_ctrl_hdr   resp_scanout;

static strict_transfer_packet cmd_transfer;
static virtio_gpu_ctrl_hdr     resp_transfer;

static virtio_gpu_resource_flush cmd_flush;
static virtio_gpu_ctrl_hdr       resp_flush;

extern "C" {

/**
 * @brief Высокоуровневая настройка графического экрана VirtIO GPU (Ресурс ID 1)
 */
int __attribute__((ms_abi)) virtio_gpu_setup_screen(void) {
    uint32_t host_width = 1024;
    uint32_t host_height = 768;


    if (os_framebuffer == nullptr) {
        os_framebuffer = (uint32_t*)kernel_alloc_pages_aligned(host_width * host_height * 4, 4096);
        if (os_framebuffer == nullptr) {
            return -15; 
        }
    }

    return 0; 
}

void virtio_gpu_redraw(void) {
    cmd_transfer.type = 0x0105; 
    cmd_transfer.flags = 0;
    cmd_transfer.fence_id = 0;
    cmd_transfer.ctx_id = 0;
    cmd_transfer.padding_hdr = 0;
    cmd_transfer.x = 0;
    cmd_transfer.y = 0;
    cmd_transfer.width = 1024;
    cmd_transfer.height = 768;
    cmd_transfer.offset = 0;
    cmd_transfer.resource_id = 2;
    cmd_transfer.padding = 0;

    virtio_dev_send_command(&my_gpu, 0, &cmd_transfer, sizeof(cmd_transfer), &resp_transfer, sizeof(resp_transfer));

    cmd_flush.hdr.type = 0x0104; 
    cmd_flush.hdr.flags = 0; 
    cmd_flush.hdr.fence_id = 0; 
    cmd_flush.hdr.ctx_id = 0; 
    cmd_flush.hdr.padding = 0;
    cmd_flush.r_x = 0;
    cmd_flush.r_y = 0;
    cmd_flush.r_width = 1024;
    cmd_flush.r_height = 768;
    cmd_flush.resource_id = 2;
    cmd_flush.padding = 0;

    virtio_dev_send_command(&my_gpu, 0, &cmd_flush, sizeof(cmd_flush), &resp_flush, sizeof(resp_flush));
}

} // extern "C"
