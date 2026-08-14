#include "virtio_pci.h"
#include "print.h"

#define DM_CURSOR_RESOURCE_ID                     3
#define DM_VIRTIO_GPU_CMD_RESOURCE_CREATE_2D      0x0101
#define DM_VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106
#define DM_VIRTIO_GPU_CMD_UPDATE_CURSOR           0x0300
#define DM_VIRTIO_GPU_RESP_OK_NODATA              0x1100

#define GPU_CONTROL_QUEUE_INDEX                   0
#define GPU_CURSOR_QUEUE_INDEX                    1
#define GPU_QUEUE_SIZE_DCA                        16

extern virtio_pci_device_t my_gpu;
extern uint64_t kernel_virtual_to_physical(void *virtual_addr);
extern void swap_buffers(void *gop);
extern void virtio_gpu_redraw(void);

#pragma pack(push, 4)
struct local_virtio_gpu_rect { uint32_t x; uint32_t y; uint32_t width; uint32_t height; };
struct local_virtio_gpu_ctrl_hdr { uint32_t type; uint32_t flags; uint64_t fence_id; uint32_t ctx_id; uint32_t padding; };
struct local_virtio_gpu_resp_display_info { struct local_virtio_gpu_ctrl_hdr hdr; struct { struct local_virtio_gpu_rect r; uint32_t enabled; uint32_t flags; } pmodes; };
struct local_virtio_gpu_transfer_to_host_2d { struct local_virtio_gpu_ctrl_hdr hdr; struct local_virtio_gpu_rect r; uint64_t offset; uint32_t resource_id; uint32_t padding; };
struct local_virtio_gpu_cursor_pos { uint32_t scanout_id; uint32_t x; uint32_t y; uint32_t padding; };
struct local_virtio_gpu_update_cursor { struct local_virtio_gpu_ctrl_hdr hdr; struct local_virtio_gpu_cursor_pos pos; uint32_t resource_id; uint32_t hot_x; uint32_t hot_y; uint32_t padding; };
#pragma pack(pop)

__attribute__((aligned(16))) static struct virtq_desc   cursor_desc_table[GPU_QUEUE_SIZE_DCA];
__attribute__((aligned(4)))  static struct virtq_avail  cursor_avail_ring;
__attribute__((aligned(4)))  static struct virtq_used   cursor_used_ring;

static uint16_t cursor_avail_idx_local = 0;

static void proto_force_log_flush(const char *msg, uint32_t y, uint8_t r, uint8_t g, uint8_t b) {
    printf(msg, 50, y, r, g, b, 255);
    swap_buffers(0);
    virtio_gpu_redraw();
}

int virtio_gpu_configure_cursor_hardware_queue(void) {
    proto_force_log_flush("[PROTO_TRACE] Starting queue configuration index 1...", 100, 255, 255, 0);

    volatile struct virtio_pci_common_cfg *cfg = (volatile struct virtio_pci_common_cfg *)my_gpu.common_cfg;
    if (!cfg) {
        proto_force_log_flush("[PROTO_TRACE] FATAL: common_cfg pointer is NULL!", 120, 255, 0, 0);
        return -1;
    }

    cfg->queue_select = GPU_CURSOR_QUEUE_INDEX;
    __asm__ volatile("" : : : "memory");

    if (cfg->queue_size == 0) {
        proto_force_log_flush("[PROTO_TRACE] FATAL: QEMU says queue 1 unavailable!", 120, 255, 0, 0);
        return -2;
    }

    cfg->queue_size = GPU_QUEUE_SIZE_DCA;

    unsigned char *p;
    p = (unsigned char*)&cursor_desc_table; for(uint32_t i=0; i<sizeof(cursor_desc_table); i++) p[i] = 0;
    p = (unsigned char*)&cursor_avail_ring; for(uint32_t i=0; i<sizeof(cursor_avail_ring); i++) p[i] = 0;
    p = (unsigned char*)&cursor_used_ring;  for(uint32_t i=0; i<sizeof(cursor_used_ring); i++)  p[i] = 0;

    uint16_t *avail_ring_array = (uint16_t*)((uintptr_t)&cursor_avail_ring + 4);
    for (int i = 0; i < GPU_QUEUE_SIZE_DCA; i++) {
        avail_ring_array[i] = i;
    }


    cfg->queue_desc   = kernel_virtual_to_physical(&cursor_desc_table);
    cfg->queue_driver = kernel_virtual_to_physical(&cursor_avail_ring);
    cfg->queue_device = kernel_virtual_to_physical(&cursor_used_ring);
    __asm__ volatile("" : : : "memory");

    proto_force_log_flush("[PROTO_TRACE] Writing configuration registers... OK.", 120, 0, 255, 0);


    cfg->queue_enable = 1;
    __asm__ volatile("" : : : "memory");

    proto_force_log_flush("[PROTO_TRACE] Queue 1 status: ENABLED.", 140, 0, 255, 0);
    return 0;
}

int virtio_dev_send_cursor_command(void *cmd_buf, uint32_t cmd_len, void *resp_buf, uint32_t resp_len) {
    (void)cmd_len; (void)resp_len;

    cursor_desc_table[0].addr = kernel_virtual_to_physical(cmd_buf);
    cursor_desc_table[0].len = sizeof(struct local_virtio_gpu_update_cursor);
    cursor_desc_table[0].flags = 1; // NEXT
    cursor_desc_table[0].next = 1;

    cursor_desc_table[1].addr = kernel_virtual_to_physical(resp_buf);
    cursor_desc_table[1].len = sizeof(struct local_virtio_gpu_ctrl_hdr);
    cursor_desc_table[1].flags = 2; // WRITE
    cursor_desc_table[1].next = 0;

    uint16_t *avail_ring_array = (uint16_t*)((uintptr_t)&cursor_avail_ring + 4);
    avail_ring_array[cursor_avail_idx_local % GPU_QUEUE_SIZE_DCA] = 0;
    
    __asm__ volatile("" : : : "memory");
    cursor_avail_ring.idx++;
    __asm__ volatile("" : : : "memory");

    cursor_avail_idx_local++;

    volatile uint32_t *notify_reg = (volatile uint32_t *)my_gpu.notify_bar;
    if (notify_reg) {
        *notify_reg = 1;
    }

    return 0;
}

