#include "virtio_pci.h"

// Низкоуровневые функции работы с портами x86_64
static inline void outdword(uint16_t port, uint32_t value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t indword(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Реализация функций чтения конфигурации PCI через механизмы портов 0xCF8/0xCFC
uint32_t pci_read_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    
    // Формируем 32-битный адрес для PCI Configuration Mechanism 1
    address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    
    outdword(0x0CF8, address);
    return indword(0x0CFC);
}

uint16_t pci_read_config_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_read_config_dword(bus, slot, func, offset);
    return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_read_config_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_read_config_dword(bus, slot, func, offset);
    return (uint8_t)((val >> ((offset & 3) * 8)) & 0xFF);
}

// Функция для получения базового адреса BAR из конфигурационного пространства PCI
static uint64_t virtio_pci_get_bar_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_index) {
    uint8_t bar_offset = 0x10 + (bar_index * 4);
    uint32_t bar_low = pci_read_config_dword(bus, slot, func, bar_offset);
    
    // Проверяем, является ли BAR 64-битным (тип 2 в битах 1-2)
    if ((bar_low & 0x06) == 0x04) {
        uint32_t bar_high = pci_read_config_dword(bus, slot, func, bar_offset + 4);
        return (((uint64_t)bar_high << 32) | (bar_low & 0xFFFFFFF0));
    }
    
    return (bar_low & 0xFFFFFFF0);
}

// Функция инициализации и маппинга структур Capabilities устройства
int virtio_pci_init_device(virtio_pci_device_t *vdev, uint8_t bus, uint8_t slot, uint8_t func) {
    (*vdev).bus = bus;
    (*vdev).slot = slot;
    (*vdev).func = func;
    (*vdev).common_cfg = 0;
    (*vdev).isr_cfg = 0;
    (*vdev).device_cfg = 0;
    (*vdev).notify_base_addr = 0;

    // Проверяем статус-регистр PCI на наличие списка Capabilities (бит 4)
    uint16_t pci_status = pci_read_config_word(bus, slot, func, 0x06);
    if ((pci_status & 0x0010) == 0) {
        return -1; // Capabilities не поддерживаются
    }

    // Читаем указатель на первую структуру из списка (смещение 0x34)
    uint8_t cap_ptr = pci_read_config_byte(bus, slot, func, 0x34);

    // Обходим список Capabilities в конфигурационном пространстве PCI
    while (cap_ptr != 0) {
        if (cap_ptr == 0xFF) {
            break;
        }

        uint8_t cap_id = pci_read_config_byte(bus, slot, func, cap_ptr);
        
        // Нас интересует только Vendor-Specific Capability (ID 0x09)
        if (cap_id == 0x09) {
            struct virtio_pci_cap cap;
            
            // Читаем базовые поля заголовка Capability
            cap.cfg_type = pci_read_config_byte(bus, slot, func, cap_ptr + 3);
            cap.bar      = pci_read_config_byte(bus, slot, func, cap_ptr + 4);
            cap.offset   = pci_read_config_dword(bus, slot, func, cap_ptr + 8);
            cap.length   = pci_read_config_dword(bus, slot, func, cap_ptr + 12);

            // Игнорируем зарезервированные или некорректные номера BAR (Раздел 4.1.4.1)
            if (cap.bar == 0 || cap.bar == 1 || cap.bar == 2 || cap.bar == 3 || cap.bar == 4 || cap.bar == 5) {
                uint64_t bar_base = virtio_pci_get_bar_addr(bus, slot, func, cap.bar);
                uint64_t target_addr = bar_base + cap.offset; // Identity mapping в UEFI

                switch (cap.cfg_type) {
                    case VIRTIO_PCI_CAP_COMMON_CFG:
                        (*vdev).common_cfg = (volatile struct virtio_pci_common_cfg *)target_addr;
                        break;
                        
                    case VIRTIO_PCI_CAP_ISR_CFG:
                        (*vdev).isr_cfg = (volatile uint8_t *)target_addr;
                        break;
                        
                    case VIRTIO_PCI_CAP_DEVICE_CFG:
                        (*vdev).device_cfg = (volatile void *)target_addr;
                        break;
                        
                    case VIRTIO_PCI_CAP_NOTIFY_CFG:
                        (*vdev).notify_base_addr = target_addr;
                        (*vdev).notify_bar = cap.bar;
                        // Считываем дополнительное поле множителя (Раздел 4.1.4.4)
                        (*vdev).notify_multiplier = pci_read_config_dword(bus, slot, func, cap_ptr + 16);
                        break;
                }
            }
        }
        // Переходим к следующему элементу списка
        cap_ptr = pci_read_config_byte(bus, slot, func, cap_ptr + 1);
    }

    // Проверяем, что критически важные структуры для Modern-интерфейса успешно найдены
    if (!(*vdev).common_cfg || !(*vdev).isr_cfg || !(*vdev).notify_base_addr) {
        return -2;
    }

    // Пошаговый алгоритм инициализации Modern-устройства (Раздел 3.1.1)
    
    // Шаг 1: Сброс устройства. Запись 0 в status сбрасывает девайс.
    (*(*vdev).common_cfg).device_status = VIRTIO_STATUS_RESET;
    
    // Ожидаем подтверждения сброса от устройства
    while ((*(*vdev).common_cfg).device_status != VIRTIO_STATUS_RESET) {
        __asm__ volatile("pause");
    }

    // Шаг 2: Выставляем статус ACKNOWLEDGE (ОС обнаружила устройство)
    (*(*vdev).common_cfg).device_status |= VIRTIO_STATUS_ACKNOWLEDGE;

    // Шаг 3: Выставляем статус DRIVER (ОС знает, как управлять устройством)
    (*(*vdev).common_cfg).device_status |= VIRTIO_STATUS_DRIVER;

    return 0; // Базовая настройка PCI-слоя завершена
}

