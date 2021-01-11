#include <Arduino.h>
#include <ble.h>

// #define BLE_DEBUG			1

#define BLE_STACK_EVT_MSG_BUF_SIZE       (sizeof(ble_evt_t) + (GATT_MTU_SIZE_DEFAULT))