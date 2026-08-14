#include "kernel.h"


volatile int has_keyboard_event = 0;
volatile int has_mouse_event = 0;
volatile uint8_t last_scancode = 0;


extern int mouse_x;
extern int mouse_y;


void sys_reset(void) {
    outb(0xCF9, 0x02);
    io_wait();
    outb(0xCF9, 0x06);
    while(1) { __asm__ volatile("hlt"); }
}


void sys_shutdown(void) {
    outw(0xB2, 0x07);
    io_wait();
    outw(0x604, 0x2000);
    io_wait();

    outl(0xCF8, 0x8000F840);
    io_wait();
    uint32_t pmbase = inl(0xCFC) & 0xFFFE;
    io_wait();

    if (pmbase != 0) {
        uint16_t pm1_cnt_port = (uint16_t)(pmbase + 0x04);
        outw(pm1_cnt_port, 0x2000 | 0x1C00);
        io_wait();
    }

    __asm__ __volatile__("cli; hlt");
}

#ifdef __cplusplus
extern "C" {
#endif
    void __attribute__((ms_abi)) screen_switch_to_virtio(void);
    int  __attribute__((ms_abi)) init_virtio_gpu(void);
    
    int  __attribute__((ms_abi)) virtio_gpu_setup_screen(void); 
#ifdef __cplusplus
}
#endif

void __attribute__((ms_abi)) kernel_main(BootInfo* info) {
    if (!info) {
        while(1) { __asm__ __volatile__("hlt"); }
    }

    init_screen_driver(info);
    fill_screen(0, 0, 0, 255);
    draw_icon(335, 115);
    swap_buffers(0);
    
    init_idt();
    init_ioapic();
    init_mouse();
    init_ahci();
    init_ext2();
    
    
    printf("[KERNEL] Initializing VirtIO GPU Transport...", 50, 100, 255, 255, 0, 255);
    int res = init_virtio_gpu();
    if (res != 0) {
        printf("FAILED!", 500, 100, 255, 0, 0, 255);
        swap_buffers(0);
        while(1) { __asm__ volatile("hlt"); }
    }
    printf("SUCCESS!", 500, 100, 0, 255, 0, 255);
    swap_buffers(0);


    printf("[KERNEL] Running virtio_gpu_setup_screen...", 50, 130, 255, 255, 0, 255);
    swap_buffers(0);
    
    int screen_res = virtio_gpu_setup_screen();
    printf("DONE!", 500, 130, 0, 255, 0, 255);
    swap_buffers(0);

    if (screen_res == 0) {

        screen_switch_to_virtio();
        

        draw_wallpaper(0, 0);
        draw_taskbar(50, 700, 923, 40, 8, 255, 255, 255, 200);
        draw_ico_folder(30, 30);
        printf("VirtIO GPU Display Setup: SUCCESS!", 50, 400, 0, 255, 0, 255);
        swap_buffers(0);
        

        virtio_gpu_redraw();


        printf("[KERNEL] Initializing Hardware Cursor Plane...", 50, 160, 255, 255, 0, 255);
        swap_buffers(0);
        display_manager_init_hardware_cursor(os_framebuffer);
        
    } else {

        printf("[KERNEL] Display Setup Error!", 50, 400, 255, 0, 0, 255);
        

        int qemu_resp = -screen_res;
        
        if (qemu_resp == 0x1200) {
            printf("QEMU RESP: VIRTIO_GPU_RESP_ERR_UNSPEC (General Device Error)", 50, 430, 255, 100, 100, 255);
        } else if (qemu_resp == 0x1201) {
            printf("QEMU RESP: VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY", 50, 430, 255, 100, 100, 255);
        } else if (qemu_resp == 0x1202) {
            printf("QEMU RESP: VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID", 50, 430, 255, 100, 100, 255);
        } else if (qemu_resp == 0x1203) {
            printf("QEMU RESP: VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID (Conflict)", 50, 430, 255, 100, 100, 255);
        } else if (qemu_resp == 0x1205) {
            printf("QEMU RESP: VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER", 50, 430, 255, 100, 100, 255);
        } else {

            if (screen_res == -15) {
                printf("OS Error: Failed to allocate framebuffer RAM (Page aligned)", 50, 430, 255, 100, 100, 255);
            } else {
                printf("Unknown status code raw register value received", 50, 430, 255, 100, 100, 255);
            }
        }
        
        swap_buffers(0);
        while(1) { __asm__ volatile("hlt"); }
    }
    
    ext2_create_dir("/", "hello_world");
    ext2_create_file("/hello_world", "hello_world.txt", 0x1A4, "Hello World from Kernel!", 24);


    for (volatile int i = 0; i < 10000000; i++) {
        __asm__ volatile("nop");
    }


    __asm__ volatile("sti");


    while (1) {
        __asm__ __volatile__("hlt");
        
        if (has_keyboard_event) {
            has_mouse_event = 0;
            has_keyboard_event = 0;
            if (last_scancode == 0x01) {
                sys_reset();
            }
        } 
        else if (has_mouse_event) {
            has_mouse_event = 0;
            
            display_manager_move_cursor(mouse_x, mouse_y);
        }
    }
}
