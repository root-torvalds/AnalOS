#ifndef VIRTIO_PCI_H
#define VIRTIO_PCI_H

#include <stdint.h>

// Константы идентификаторов PCI для VirtIO
#define VIRTIO_PCI_VENDOR_ID         0x1AF4
#define VIRTIO_PCI_DEVICE_ID_GPU     0x1050 // Modern GPU (0x1040 + 16)

// Константы типов структур VirtIO Capabilities (cfg_type)
#define VIRTIO_PCI_CAP_COMMON_CFG    1
#define VIRTIO_PCI_CAP_NOTIFY_CFG    2
#define VIRTIO_PCI_CAP_ISR_CFG       3
#define VIRTIO_PCI_CAP_DEVICE_CFG    4
#define VIRTIO_PCI_CAP_PCI_CFG       5

// Константы статуса устройства (device_status)
#define VIRTIO_STATUS_RESET          0
#define VIRTIO_STATUS_ACKNOWLEDGE    1
#define VIRTIO_STATUS_DRIVER         2
#define VIRTIO_STATUS_FEATURES_OK    8
#define VIRTIO_STATUS_DRIVER_OK      4
#define VIRTIO_STATUS_FAILED         128

// Структура PCI Capability заголовка для VirtIO (Раздел 4.1.4)
#pragma pack(push, 1)
struct virtio_pci_cap {
    uint8_t  cap_vndr;    // Смешение 0x09 (Vendor Specific)
    uint8_t  cap_next;    // Указатель на следующую структуру
    uint8_t  cap_len;     // Полная длина структуры
    uint8_t  cfg_type;    // Тип (VIRTIO_PCI_CAP_*)
    uint8_t  bar;         // Номер BAR (0-5)
    uint8_t  id;          // Уникальный ID структуры
    uint8_t  padding[2];  // Выравнивание до dword
    uint32_t offset;     // Смещение внутри BAR
    uint32_t length;     // Длина структуры в байтах
};

// Расширение для структуры уведомлений (Раздел 4.1.4.4)
struct virtio_pci_notify_cap {
    struct virtio_pci_cap cap;
    uint32_t notify_off_multiplier;
};

// Структура общей конфигурации VirtIO Modern (Раздел 4.1.4.3)
struct virtio_pci_common_cfg {
    uint32_t device_feature_select;
    uint32_t device_feature;
    uint32_t driver_feature_select;
    uint32_t driver_feature;
    uint16_t config_msix_vector;
    uint16_t num_queues;
    uint8_t  device_status;
    uint8_t  config_generation;

    uint16_t queue_select;
    uint16_t queue_size;
    uint16_t queue_msix_vector;
    uint16_t queue_enable;
    uint16_t queue_notify_off;
    uint64_t queue_desc;
    uint64_t queue_driver;
    uint64_t queue_device;
    uint16_t queue_notify_data;
    uint16_t queue_reset;
};
#pragma pack(pop)

// Контекст инициализированного VirtIO PCI транспортного слоя
typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    
    volatile struct virtio_pci_common_cfg *common_cfg;
    volatile uint8_t                      *isr_cfg;
    volatile void                         *device_cfg;
    
    uint64_t notify_base_addr;
    uint32_t notify_multiplier;
    uint8_t  notify_bar;
} virtio_pci_device_t;

// Прототипы функций внешнего интерфейса
int virtio_pci_init_device(virtio_pci_device_t *vdev, uint8_t bus, uint8_t slot, uint8_t func);

#endif // VIRTIO_PCI_H

