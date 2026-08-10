#include "virtio_pci.h"
#include "virtio_gpu_cmd.hpp"

// Глобальный указатель на фреймбуфер нашей ОС
uint32_t *os_framebuffer = nullptr;

// Импортируем Си-контекст устройства и функции транспорта из virtio_pci.c
extern "C" {
    extern virtio_pci_device_t my_gpu;
    extern int virtio_dev_send_command(virtio_pci_device_t *vdev, uint16_t queue_index, void *cmd_buf, uint32_t cmd_len, void *resp_buf, uint32_t resp_len);
    extern uint64_t kernel_virtual_to_physical(void *virtual_addr);
    extern void* kernel_alloc_pages_aligned(uint32_t size, uint32_t alignment);
}

// Экспортируем функции графического протокола для Си-кода ядра
extern "C" {

int virtio_gpu_setup_screen(void) {
    if (os_framebuffer == nullptr) {
        os_framebuffer = (uint32_t*)kernel_alloc_pages_aligned(DISPLAY_WIDTH * DISPLAY_HEIGHT * 4, 4096);
        if (os_framebuffer == nullptr) {
            return -15; 
        }
    }

    virtio_gpu_ctrl_hdr resp_create;
    virtio_gpu_ctrl_hdr resp_attach;
    virtio_gpu_set_scanout resp_scanout;

    // ШАГ 1: Создаем 2D холст внутри видеокарты QEMU
    virtio_gpu_resource_create_2d cmd_create;
    cmd_create.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    cmd_create.hdr.flags = 0; cmd_create.hdr.fence_id = 0; cmd_create.hdr.ctx_id = 0; cmd_create.hdr.padding = 0;
    cmd_create.resource_id = GPU_RESOURCE_ID;
    cmd_create.format = 3; 
    cmd_create.width = DISPLAY_WIDTH;
    cmd_create.height = DISPLAY_HEIGHT;

    virtio_dev_send_command(&my_gpu, 0, &cmd_create, sizeof(cmd_create), &resp_create, sizeof(resp_create));
    if (resp_create.type != VIRTIO_GPU_RESP_OK_NODATA) {
        if (resp_create.type == 0) return -10;
        return (int)resp_create.type; 
    }

    // ШАГ 2: Передаем физический адрес os_framebuffer хосту
    struct __attribute__((packed, aligned(4))) AttachPacket {
        virtio_gpu_resource_attach_backing attach;
        virtio_gpu_mem_entry entry;
    } cmd_attach_packet;

    cmd_attach_packet.attach.hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    cmd_attach_packet.attach.hdr.flags = 0; cmd_attach_packet.attach.hdr.fence_id = 0; cmd_attach_packet.attach.hdr.ctx_id = 0; cmd_attach_packet.attach.hdr.padding = 0;
    cmd_attach_packet.attach.resource_id = GPU_RESOURCE_ID;
    cmd_attach_packet.attach.nr_entries = 1;

    cmd_attach_packet.entry.addr = kernel_virtual_to_physical(os_framebuffer);
    cmd_attach_packet.entry.length = DISPLAY_WIDTH * DISPLAY_HEIGHT * 4;
    cmd_attach_packet.entry.padding = 0;

    virtio_dev_send_command(&my_gpu, 0, &cmd_attach_packet, sizeof(cmd_attach_packet), &resp_attach, sizeof(resp_attach));
    if (resp_attach.type != VIRTIO_GPU_RESP_OK_NODATA) {
        return -11; 
    }

    // ШАГ 3: Привязываем настроенный 2D ресурс к Scanout 0
    virtio_gpu_set_scanout cmd_scanout;
    cmd_scanout.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    cmd_scanout.hdr.flags = 0; cmd_scanout.hdr.fence_id = 0; cmd_scanout.hdr.ctx_id = 0; cmd_scanout.hdr.padding = 0;
    cmd_scanout.r_x = 0; cmd_scanout.r_y = 0; cmd_scanout.r_width = DISPLAY_WIDTH; cmd_scanout.r_height = DISPLAY_HEIGHT;
    cmd_scanout.scanout_id = 0;
    cmd_scanout.resource_id = GPU_RESOURCE_ID;

    virtio_dev_send_command(&my_gpu, 0, &cmd_scanout, sizeof(cmd_scanout), &resp_scanout, sizeof(resp_scanout));
    if (resp_scanout.hdr.type != VIRTIO_GPU_RESP_OK_NODATA) {
        return -12; 
    }

    return 0; 
}

void virtio_gpu_redraw(void) {
    virtio_gpu_ctrl_hdr resp_flush;
    virtio_gpu_resource_flush cmd_flush;
    
    cmd_flush.hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    cmd_flush.hdr.flags = 0; cmd_flush.hdr.fence_id = 0; cmd_flush.hdr.ctx_id = 0; cmd_flush.hdr.padding = 0;
    cmd_flush.r_x = 0; cmd_flush.r_y = 0; cmd_flush.r_width = DISPLAY_WIDTH; cmd_flush.r_height = DISPLAY_HEIGHT;
    cmd_flush.resource_id = GPU_RESOURCE_ID;
    cmd_flush.padding = 0;

    virtio_dev_send_command(&my_gpu, 0, &cmd_flush, sizeof(cmd_flush), &resp_flush, sizeof(resp_flush));
}

} // extern "C"

