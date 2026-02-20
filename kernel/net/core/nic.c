#include <net/nic.h>

nic_driver_t* current_nic = NULL;

void nic_register_driver(nic_driver_t* driver) {
    if (driver) {
        current_nic = driver;
    }
}
