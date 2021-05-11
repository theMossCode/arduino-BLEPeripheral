#include <Arduino.h>
#include "BLECommon.h"
#include "BLERemoteCharacteristic.h"
#include "BLERemoteService.h"
#include "BLEUuid.h"
#include "BLECentral.h"


#define BLE_CENTRAL_ROLE_DEBUG  1

#define DEFAULT_SCAN_WINDOW			((500*16)/10)// 500ms
#define DEFAULT_SCAN_INTERVAL		((10000*16)/10) //10s
#define DEFAULT_SCAN_TIMEOUT		0
#define DEFAULT_MAX_CONN_INTERVAL	((450*8)/10)// 450ms
#define DEFAULT_MIN_CONN_INTERVAL	((150*8)/10)//150ms
#define DEFAULT_SLAVE_LATENCY		3
#define DEFAULT_CONN_SUP_TIMEOUT	6000/10 //6s

#define P256_KEY_LEN				32


// Callback typedefs
typedef void (*BLEScanEventHandler)(ble_gap_addr_t* addr, uint8_t* data, uint8_t dataLen, int8_t rssi);
typedef void (*BLEConnectedEventHandler)(ble_gap_addr_t* peerAddress);
typedef void (*BLEDisconnectedEventHandler)(uint8_t disconnectReason);
typedef void (*BLETimeoutEventHandler)(uint8_t timeoutSource);
typedef void (*BLEConnParamUpdateHandler)(ble_gap_conn_params_t *params);

typedef void (*BLEServicesDiscoveredCB)(uint16_t status, uint16_t count, ble_gattc_service_t *services);
typedef void (*BLECharacteristicsDiscoveredCB)(uint16_t status, uint16_t count, ble_gattc_char_t *chr);
typedef void (*BLEDescriptorsDiscoveredCB)(uint16_t status, uint16_t count, ble_gattc_desc_t *desc);
typedef void (*BLEAttrDiscoveryCompleteCB)();

enum BLECentralRoleEvent{
	BLE_CENTRAL_CONNECTED, BLE_CENTRAL_DISCONNECTED, BLE_CENTRAL_ADV_REPORT_RECIEVED, BLE_CENTRAL_TIMEOUT, BLE_CENTRAL_SERVICES_DISCOVERED,
	BLE_CENTRAL_CHARACTERISTICS_DISCOVERED, BLE_CENTRAL_DESCRIPTORS_DISCOVERED
};


#if BLE_CENTRAL_ROLE_DEBUG
enum ble_debug_op_codes{
	BLE_DEBUG_OP_CONNECT, BLE_DEBUG_OP_DISCONNECT, BLE_DEBUG_OP_VS_UUID_ADD, BLE_DEBUG_OP_START_SCAN, BLE_DEBUG_OP_STOP_SCAN, BLE_DEBUG_OP_DISC_PRIM_SERVICES, BLE_DEBUG_OP_DISC_CHR,
	BLE_DEBUG_OP_DISC_DESC, BLE_DEBUG_OP_READ_CHR, BLE_DEBUG_OP_WRITE_CHR, BLE_DEBUG_OP_TX_COMPLETE
};

typedef void (*BLEDebugEventHandler)(int debug_op_code, uint32_t errCode, uint8_t *custom_data);
#endif // BLE_CENTRAL_ROLE_DEBUG

class BLECentralRole: public BLERemoteCharacteristicValueChangeListener, public BLECentral
{
public:
	BLECentralRole();

	// Scanning
	void setScanWindow(uint16_t window);
	void setScanInterval(uint16_t interval);
 	void setScanTimeout(uint16_t timeout);
	void setScanActive(bool activeScan);

	uint32_t startScan();
	uint32_t stopScan();


	// Connection
	void setConnMaxInterval(uint16_t maxInterval);
	void setConnMinInterval(uint16_t minInterval);
	void setConnSlaveLatency(uint16_t slaveLatency);
	void setConnSupTimeout(uint16_t connSupTimeout);

	uint32_t connect(ble_gap_addr_t* addr);
	uint32_t discoverServices();
	uint32_t cancelConnection();
	uint32_t disconnect();


	// GATTC Methods
	void addRemoteAttribute(BLERemoteAttribute& attribute);


	// Central Role Methods
	bool begin();
	void poll(uint32_t* evtBuf, uint16_t* evtLen);
	uint32_t end();

	// Callbacks
	// gap
	void setScanEventHandler(BLEScanEventHandler eventHandler);
	void setConnectedEventHandler(BLEConnectedEventHandler eventHandler);
	void setDisconnectedEventHandler(BLEDisconnectedEventHandler eventHandler);
	void setTimeoutEventHandler(BLETimeoutEventHandler eventHandler);
	void setConnectionParamUpdateEventHandler(BLEConnParamUpdateHandler eventHandler);

	// Discovery
	void setGattcServicesDiscoveredEventHandler(BLEServicesDiscoveredCB eventHandler);
	void setGattcCharacteristicsDiscoveredEventHandler(BLECharacteristicsDiscoveredCB eventHandler);
	void setGattcDescriptorsDiscoveredEventHandler(BLEDescriptorsDiscoveredCB eventHandler);
	void setGattcAttributeDiscoveryCompleteEventHandler(BLEAttrDiscoveryCompleteCB eventHandler);

	//Debug
	#if BLE_CENTRAL_ROLE_DEBUG
	void setDebugHandler(BLEDebugEventHandler handler);
	#endif

protected:
    struct remoteServiceInfo {
      BLERemoteService *service;// Need this to initialise service using string uuid
	  ble_uuid_t uuid;
      ble_gattc_handle_range_t handles_range;
    };

    struct remoteCharacteristicInfo {
      BLERemoteCharacteristic *characteristic;
	  struct remoteServiceInfo *remoteService;
	  ble_uuid_t uuid;
	  ble_gatt_char_props_t properties;
	  uint16_t cccd_handle;
      uint16_t value_handle;
    };

	struct gattc_discovery_callbacks{
		BLEServicesDiscoveredCB services_cb;
		BLECharacteristicsDiscoveredCB chr_cb;
		BLEDescriptorsDiscoveredCB desc_cb;
		BLEAttrDiscoveryCompleteCB complete_cb;
	};

	struct connection_callbacks{
		BLEScanEventHandler _scanEventHandler;
		BLEConnectedEventHandler _connectedEventHandler;
		BLEDisconnectedEventHandler _disconnectedEventHandler;
		BLETimeoutEventHandler _timeoutEventHandler;
		BLEConnParamUpdateHandler _connParamUpdateHandler;		
	};

private:
	uint16_t _connectionHandle;
	ble_gap_addr_t peerAddress;
	uint8_t tx_buffer_count;
	uint8_t remote_request_in_progress;

	uint16_t _scanInterval;
	uint16_t _scanWindow;
	uint8_t _activeScan;
	uint16_t _scanTimeout;

	uint16_t _minConnInterval;
	uint16_t _maxConnInterval;
	uint16_t _slaveLatency;
	uint16_t _connSupTimeout;

	#if BLE_CENTRAL_ROLE_DEBUG
	BLEDebugEventHandler debug_handler;
	#endif

	struct connection_callbacks _connection_callbacks;
	ble_gap_scan_params_t _scanParams;
	ble_gap_conn_params_t _connParams;
	
	// GATTC
	// GATTC structs and variables
	// We do not expect any more than 10 service and characteristics, so this will work for now
	struct remoteServiceInfo _remoteServiceInfo[10];
	struct remoteCharacteristicInfo _remoteCharacteristicInfo[10];
	struct gattc_discovery_callbacks _discovery_callbacks;

	// discovery variables
	uint8_t _remoteServiceDiscoveryIndex;
	uint8_t _numRemoteServices;
	uint8_t _numRemoteCharacteristics;

	// Initialisation Functions
	void init_attributes();
	void init_callbacks();

	// GATTC private functions
	// On Discovery Functions
	void on_services_discovered(ble_gattc_evt_prim_srvc_disc_rsp_t *resp);
	void on_characteristics_discovered(ble_gattc_evt_char_disc_rsp_t *resp);
	void on_descriptors_discovered(ble_gattc_evt_desc_disc_rsp_t *resp);

	void gap_loop(ble_evt_t *evt);
	void gattc_loop(ble_evt_t *evt);

	// Value update functions, inherited from BLERemoteCharacteristic
    virtual bool canReadRemoteCharacteristic(BLERemoteCharacteristic& /*characteristic*/);
    virtual bool readRemoteCharacteristic(BLERemoteCharacteristic& /*characteristic*/);
    virtual bool canWriteRemoteCharacteristic(BLERemoteCharacteristic& /*characteristic*/);
    virtual bool writeRemoteCharacteristic(BLERemoteCharacteristic& /*characteristic*/, const unsigned char /*value*/[], unsigned char /*length*/);
    virtual bool canSubscribeRemoteCharacteristic(BLERemoteCharacteristic& /*characteristic*/);
    virtual bool subscribeRemoteCharacteristic(BLERemoteCharacteristic& /*characteristic*/);
    virtual bool canUnsubscribeRemoteCharacteristic(BLERemoteCharacteristic& /*characteristic*/);
    virtual bool unsubcribeRemoteCharacteristic(BLERemoteCharacteristic& /*characteristic*/);

	// get nordic uuid from char uuid
	ble_uuid_t get_nordic_uuid(const char *uuid);

	// flag to indicate aatribute discovery complete
	bool attribute_discovery_complete;

	// for debug purposes, remove this later
	uint16_t last_op_handle;
	uint8_t discovered_services;
	uint8_t discovered_chr;
};