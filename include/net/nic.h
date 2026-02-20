#ifndef ETEROS_NET_NIC_H
#define ETEROS_NET_NIC_H

#include "../types.h"

/**
 * Interface for Network Interface Card (NIC) drivers.
 * Decouples the network stack from specific hardware implementations.
 */
typedef struct nic_driver {
    const char* name;

    /**
     * Initialize the driver.
     * @param dev Pointer to device structure (e.g., pci_device_t*), or NULL.
     * @return 0 on success, < 0 on error.
     */
    int (*init)(void* dev);

    /**
     * Send a raw packet.
     * @param data Pointer to the packet data.
     * @param len Length of the packet.
     * @return 0 on success, < 0 on error.
     */
    int (*send)(const void* data, uint16_t len);

    /**
     * Receive a packet (Polling).
     * @param buffer Buffer to store the received packet.
     * @param max_len Maximum length of the buffer.
     * @return Length of received packet, or 0 if none.
     */
    int (*receive)(void* buffer, uint16_t max_len);

    /**
     * Get the MAC address of the device.
     * @return Pointer to 6-byte MAC address array.
     */
    uint8_t* (*get_mac)(void);

} nic_driver_t;

/* Global pointer to the active NIC driver */
extern nic_driver_t* current_nic;

/**
 * Register a NIC driver as the active network interface.
 * @param driver Pointer to the driver structure.
 */
void nic_register_driver(nic_driver_t* driver);

#endif /* ETEROS_NET_NIC_H */
