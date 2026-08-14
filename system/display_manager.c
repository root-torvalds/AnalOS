#include "display_manager.h"
#include "virtio_pci.h"
#include "print.h"


#define DM_SCREEN_RESOURCE_ID                  2 
#define DM_CURSOR_RESOURCE_ID                  3 

#define DM_CMD_RESOURCE_CREATE_2D              0x0101
#define DM_CMD_RESOURCE_ATTACH_BACKING         0x0106
#define DM_CMD_SET_SCANOUT                     0x0103
#define DM_CMD_TRANSFER_TO_HOST_2D             0x0105
#define DM_CMD_RESOURCE_FLUSH                  0x0104
#define DM_CMD_UPDATE_CURSOR                   0x0300
#define DM_RESP_OK_NODATA                      0x1100
#define DM_FLAG_FENCE                          1

extern virtio_pci_device_t my_gpu;
extern uint64_t kernel_virtual_to_physical(void *virtual_addr);
extern int virtio_dev_send_command(virtio_pci_device_t *vdev, uint16_t queue_index, void *cmd_buf, uint32_t cmd_len, void *resp_buf, uint32_t resp_len);
extern int virtio_dev_send_cursor_command(void *cmd_buf, uint32_t cmd_len, void *resp_buf, uint32_t resp_len);
extern void swap_buffers(void *gop);
extern void virtio_gpu_redraw(void);


__attribute__((aligned(4096))) static uint8_t hardware_cursor_buffer[64 * 64 * 4];
static uint64_t cursor_fence_counter = 1;


#pragma pack(push, 4)

struct dm_gpu_rect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

struct dm_transfer_to_host_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    struct dm_gpu_rect r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
};

struct dm_resource_flush {
    struct virtio_gpu_ctrl_hdr hdr;
    struct dm_gpu_rect r;
    uint32_t resource_id;
    uint32_t padding;
};

struct dm_cursor_pos {
    uint32_t scanout_id;
    uint32_t x;
    uint32_t y;
    uint32_t padding;
};

struct dm_update_cursor {
    struct virtio_gpu_ctrl_hdr hdr;
    struct dm_cursor_pos pos;
    uint32_t resource_id;
    uint32_t hot_x;
    uint32_t hot_y;
    uint32_t padding;
};

struct dm_cursor_attach_packet {
    struct virtio_gpu_resource_attach_backing attach;
    struct virtio_gpu_mem_entry entry;
};

#pragma pack(pop)

static struct dm_update_cursor global_dma_cursor_cmd;


void dm_force_log_flush(const char *msg, uint32_t y, uint8_t r, uint8_t g, uint8_t b) {
    printf(msg, 50, y, r, g, b, 255);
    swap_buffers(0);
    virtio_gpu_redraw();
}

int display_manager_init_hardware_cursor(void *cursor_rgba_buffer) {
    (void)cursor_rgba_buffer;

    dm_force_log_flush("[DM_TRACE] Inside display_manager_init...", 190, 0, 255, 255);

    static const char cursor16[16][16] = {
     {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
     {1,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0}, {1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0},
     {1,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0}, {1,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0},
     {1,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0}, {1,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0},
     {1,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0}, {1,2,2,2,2,2,1,1,1,1,0,0,0,0,0,0},
     {1,2,2,1,2,2,1,0,0,0,0,0,0,0,0,0}, {1,2,1,0,1,2,2,1,0,0,0,0,0,0,0,0},
     {1,1,0,0,1,2,2,1,0,0,0,0,0,0,0,0}, {0,0,0,0,0,1,2,2,1,0,0,0,0,0,0,0},
     {0,0,0,0,0,1,2,2,1,0,0,0,0,0,0,0}, {0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0}
    };

    uint32_t *dst = (uint32_t*)hardware_cursor_buffer;
    for (int i = 0; i < 64 * 64; i++) dst[i] = 0x00000000;

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            uint32_t pixel_index = y * 64 + x;
            char val = cursor16[y][x];
            if (val == 1) dst[pixel_index] = 0xFF000000;
            else if (val == 2) dst[pixel_index] = 0xFFFFFFFF;
        }
    }

    struct virtio_gpu_ctrl_hdr resp_hdr;
    int status;

    unsigned char *raw_cmd = (unsigned char *)&global_dma_cursor_cmd;
    for (uint32_t i = 0; i < sizeof(global_dma_cursor_cmd); i++) raw_cmd[i] = 0;


    struct virtio_gpu_resource_create_2d cmd_c2d;
    unsigned char *raw_c2d = (unsigned char *)&cmd_c2d;
    for (uint32_t i = 0; i < sizeof(cmd_c2d); i++) raw_c2d[i] = 0;

    cmd_c2d.hdr.type = DM_CMD_RESOURCE_CREATE_2D;
    cmd_c2d.resource_id = DM_CURSOR_RESOURCE_ID;
    cmd_c2d.format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;
    cmd_c2d.width = 64;
    cmd_c2d.height = 64;

    dm_force_log_flush("[DM_TRACE] Step 1: Sending CMD_RESOURCE_CREATE_2D...", 220, 255, 255, 0);
    status = virtio_dev_send_command(&my_gpu, 0, &cmd_c2d, sizeof(cmd_c2d), &resp_hdr, sizeof(resp_hdr));
    
    if (status != 0 || resp_hdr.type != DM_RESP_OK_NODATA) {
        dm_force_log_flush("[DM_TRACE] Step 1 FATAL: QEMU Refused Resource Create!", 240, 255, 0, 0);
        return -1;
    }
    dm_force_log_flush("[DM_TRACE] Step 1 SUCCESS! Resource ID=3 Active.", 240, 0, 255, 0);


    struct dm_cursor_attach_packet packet;
    unsigned char *raw_packet = (unsigned char *)&packet;
    for (uint32_t i = 0; i < sizeof(packet); i++) raw_packet[i] = 0;

    packet.attach.hdr.type = DM_CMD_RESOURCE_ATTACH_BACKING;
    packet.attach.resource_id = DM_CURSOR_RESOURCE_ID;
    packet.attach.nr_entries = 1;
    packet.entry.addr = kernel_virtual_to_physical(hardware_cursor_buffer);
    packet.entry.length = 64 * 64 * 4;

    dm_force_log_flush("[DM_TRACE] Step 2: Sending ATTACH_BACKING...", 270, 255, 255, 0);
    status = virtio_dev_send_command(&my_gpu, 0, &packet, sizeof(packet), &resp_hdr, sizeof(resp_hdr));
    if (status != 0 || resp_hdr.type != DM_RESP_OK_NODATA) {
        dm_force_log_flush("[DM_TRACE] Step 2 FATAL: Memory mapping rejected!", 290, 255, 0, 0);
        return -2;
    }
    dm_force_log_flush("[DM_TRACE] Step 2 SUCCESS! Memory backing locked.", 290, 0, 255, 0);


    resp_hdr.type = 0; 
    global_dma_cursor_cmd.hdr.type = DM_CMD_UPDATE_CURSOR;
    global_dma_cursor_cmd.hdr.flags = DM_FLAG_FENCE; 
    global_dma_cursor_cmd.hdr.fence_id = cursor_fence_counter++;
    global_dma_cursor_cmd.hdr.ctx_id = 0;

    global_dma_cursor_cmd.pos.scanout_id = 0; 
    global_dma_cursor_cmd.pos.x = 200; 
    global_dma_cursor_cmd.pos.y = 200;
    
    global_dma_cursor_cmd.resource_id = DM_CURSOR_RESOURCE_ID;
    global_dma_cursor_cmd.hot_x = 0;
    global_dma_cursor_cmd.hot_y = 0;

    dm_force_log_flush("[DM_TRACE] Step 3: Sending асинхронный UPDATE_CURSOR...", 320, 255, 255, 0);
    status = virtio_dev_send_cursor_command(&global_dma_cursor_cmd, sizeof(global_dma_cursor_cmd), &resp_hdr, sizeof(resp_hdr));
    
    dm_force_log_flush("[DM_TRACE] Step 3 DONE! Fast Track command pipeline active.", 340, 0, 255, 255);
    return status;
}

int display_manager_move_cursor(uint32_t x, uint32_t y) {
    struct virtio_gpu_ctrl_hdr resp_hdr;
    global_dma_cursor_cmd.hdr.type = DM_CMD_UPDATE_CURSOR;
    global_dma_cursor_cmd.hdr.flags = DM_FLAG_FENCE; 
    global_dma_cursor_cmd.hdr.fence_id = cursor_fence_counter++;
    global_dma_cursor_cmd.pos.x = x;
    global_dma_cursor_cmd.pos.y = y;
    return virtio_dev_send_cursor_command(&global_dma_cursor_cmd, sizeof(global_dma_cursor_cmd), &resp_hdr, sizeof(resp_hdr));
}

int display_manager_update_region(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (x >= 1024 || y >= 768 || w == 0 || h == 0) return 0;
    if (x + w > 1024) w = 1024 - x;
    if (y + h > 768) h = 768 - y;
    struct dm_transfer_to_host_2d cmd_trans;
    struct dm_resource_flush cmd_flush;
    struct virtio_gpu_ctrl_hdr resp_hdr;
    int status;
    unsigned char *raw_trans = (unsigned char *)&cmd_trans;
    for (uint32_t i = 0; i < sizeof(cmd_trans); i++) raw_trans[i] = 0;
    

    cmd_trans.hdr.type = DM_CMD_TRANSFER_TO_HOST_2D;
    cmd_trans.r.x = x; cmd_trans.r.y = y; cmd_trans.r.width = w; cmd_trans.r.height = h;
    cmd_trans.offset = (uint64_t)((y * 1024 + x) * 4);
    cmd_trans.resource_id = DM_SCREEN_RESOURCE_ID;
    
    status = virtio_dev_send_command(&my_gpu, 0, &cmd_trans, sizeof(cmd_trans), &resp_hdr, sizeof(resp_hdr));
    if (status != 0 || resp_hdr.type != DM_RESP_OK_NODATA) return -1;
    
    unsigned char *raw_flush = (unsigned char *)&cmd_flush;
    for (uint32_t i = 0; i < sizeof(cmd_flush); i++) raw_flush[i] = 0;
    

    cmd_flush.hdr.type = DM_CMD_RESOURCE_FLUSH;
    cmd_flush.r.x = x; cmd_flush.r.y = y; cmd_flush.r.width = w; cmd_flush.r.height = h;
    cmd_flush.resource_id = DM_SCREEN_RESOURCE_ID;
    return virtio_dev_send_command(&my_gpu, 0, &cmd_flush, sizeof(cmd_flush), &resp_hdr, sizeof(resp_hdr));
}

