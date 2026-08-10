#include "kernel.h"

// Глобальный контекст устройства (аллоцирован в pci.c)
extern virtio_pci_device_t my_gpu;

#ifdef __cplusplus
extern "C" {
#endif

// ЧАСТЬ 2: Оркестровка этапов инициализации (Раздел 3.1.1)
int init_virtio_gpu(void) {
    // Координаты графического адаптера на шине PCI в QEMU q35
    uint8_t bus = 0;
    uint8_t slot = 1;
    uint8_t func = 0;

    // Шаг 1-3: Поиск устройства и начальный взвод битов статуса
    int res = virtio_pci_init_device(&my_gpu, bus, slot, func);
    if (res != 0) {
        return res; 
    }

    // Шаг 4-6: Согласование возможностей драйвера и хоста
    res = virtio_gpu_negotiate_features(&my_gpu);
    if (res != 0) {
        return res; 
    }

    // Шаг 7: Выделение колец для Virtqueues
    // Настраиваем controlq для отправки пакетов
    res = virtio_queue_setup(&my_gpu, 0);
    if (res != 0) {
        return res;
    }

    // Настраиваем cursorq для мыши
    res = virtio_queue_setup(&my_gpu, 1);
    if (res != 0) {
        return res;
    }

    // Шаг 8: Взводим DRIVER_OK. Видеокарта запущена!
    (*my_gpu.common_cfg).device_status |= VIRTIO_STATUS_DRIVER_OK;
    return 0; 
}

#ifdef __cplusplus
}
#endif

