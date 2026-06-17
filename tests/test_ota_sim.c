#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#define UPDATE_STATE_NONE    0
#define UPDATE_STATE_PENDING 1
#define UPDATE_STATE_SUCCESS 2
#define UPDATE_STATE_FAILED  3

static uint8_t mock_nvram_boot_partition = 0;
static uint8_t mock_nvram_update_state = UPDATE_STATE_NONE;

void nvram_set_boot_partition(uint8_t partition_id) {
    mock_nvram_boot_partition = partition_id;
}

uint8_t nvram_get_boot_partition(void) {
    if (mock_nvram_boot_partition != 0 && mock_nvram_boot_partition != 1) {
        return 0xFF;
    }
    return mock_nvram_boot_partition;
}

void nvram_set_update_state(uint8_t state) {
    mock_nvram_update_state = state;
}

uint8_t nvram_get_update_state(void) {
    if (mock_nvram_update_state > UPDATE_STATE_FAILED) {
        return UPDATE_STATE_NONE;
    }
    return mock_nvram_update_state;
}

void simulate_update() {
    if (nvram_get_update_state() == UPDATE_STATE_PENDING) {
        printf("ERROR: Update already pending.\n");
        return;
    }
    uint8_t current = nvram_get_boot_partition();
    uint8_t next = (current == 0) ? 1 : 0;

    // Simulate write success
    nvram_set_boot_partition(next);
    nvram_set_update_state(UPDATE_STATE_PENDING);
}

void simulate_confirm() {
    if (nvram_get_update_state() == UPDATE_STATE_PENDING) {
        nvram_set_update_state(UPDATE_STATE_SUCCESS);
    }
}

// Simulates partition_get_active_root logic
typedef struct {
    uint8_t impl;
} fs_node_t;

fs_node_t* partition_get_active_root() {
    fs_node_t *node = malloc(sizeof(fs_node_t));
    uint8_t nvram_part = nvram_get_boot_partition();
    if (nvram_part != 0xFF) {
        node->impl = nvram_part;
    } else {
        node->impl = 0; // Default active
    }
    return node;
}

void simulate_rollback() {
    if (nvram_get_update_state() == UPDATE_STATE_PENDING) {
        fs_node_t *active = partition_get_active_root();
        uint8_t current = active->impl;
        uint8_t next = (current == 0) ? 1 : 0;
        nvram_set_boot_partition(next);
        nvram_set_update_state(UPDATE_STATE_FAILED);
        free(active);
    }
}


int main() {
    printf("Starting OTA state machine tests...\n");

    // Test 1: Initial state
    assert(nvram_get_boot_partition() == 0);
    assert(nvram_get_update_state() == UPDATE_STATE_NONE);

    // Test 2: Apply update
    simulate_update();
    assert(nvram_get_boot_partition() == 1);
    assert(nvram_get_update_state() == UPDATE_STATE_PENDING);

    // Test 3: Confirm update
    simulate_confirm();
    assert(nvram_get_boot_partition() == 1);
    assert(nvram_get_update_state() == UPDATE_STATE_SUCCESS);

    // Test 4: Apply another update
    simulate_update();
    assert(nvram_get_boot_partition() == 0);
    assert(nvram_get_update_state() == UPDATE_STATE_PENDING);

    // Test 5: Rollback
    simulate_rollback();
    assert(nvram_get_boot_partition() == 1);
    assert(nvram_get_update_state() == UPDATE_STATE_FAILED);

    // Test 6: Try to apply update while pending (should fail)
    nvram_set_update_state(UPDATE_STATE_PENDING);
    uint8_t before_part = nvram_get_boot_partition();
    simulate_update();
    assert(nvram_get_boot_partition() == before_part); // Should not change
    assert(nvram_get_update_state() == UPDATE_STATE_PENDING);

    // Test 7: Rollback when not pending (should fail/no-op)
    nvram_set_update_state(UPDATE_STATE_SUCCESS);
    before_part = nvram_get_boot_partition();
    simulate_rollback();
    assert(nvram_get_boot_partition() == before_part);
    assert(nvram_get_update_state() == UPDATE_STATE_SUCCESS);

    // Test 8: Uninitialized NVRAM values
    mock_nvram_boot_partition = 0xAA;
    mock_nvram_update_state = 0xBB;
    assert(nvram_get_boot_partition() == 0xFF);
    assert(nvram_get_update_state() == UPDATE_STATE_NONE);

    // Rollback with uninitialized boot partition
    nvram_set_update_state(UPDATE_STATE_PENDING);
    simulate_rollback();
    // Default active root impl is 0, so next should be 1
    assert(nvram_get_boot_partition() == 1);
    assert(nvram_get_update_state() == UPDATE_STATE_FAILED);

    printf("All OTA state machine tests passed!\n");
    return 0;
}
