#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../efi.h"

extern volatile int has_mouse_event;
extern volatile uint8_t last_scancode;

#include "idt.h"
#include "lib.h"
#include "ahci.h"
#include "ext2.h"
#include "mouse.h"
#include "print.h"
#include "screen.h"
#include "allocate.h"
#include "keyboard.h"
#include "virtio_pci.h"
#include "virtio_gpu.h"
//#include "virtio_queue.h"
//#include "virtio_gpu_proto.h"
#include "display_manager.h"
#ifdef __cplusplus
}
#endif

#include "virtio_gpu_cmd.hpp"
