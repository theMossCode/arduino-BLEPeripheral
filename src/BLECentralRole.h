#include <Arduino.h>
#include "BLECommon.h"

#define DEFAULT_SCAN_WINDOW			((500*16)/10)// 500ms
#define DEFAULT_SCAN_INTERVAL		((10000*16)/10) //10s
#define DEFAULT_SCAN_TIMEOUT		0
#define DEFAULT_MAX_CONN_INTERVAL	800 
#define DEFAULT_MIN_CONN_INTERVAL	400 
#define DEFAULT_SLAVE_LATENCY		0 
#define DEFAULT_CONN_SUP_TIMEOUT	5000/10 //5s

typedef void (*BLEScanEventHandler)(ble_gap_addr_t* addr, uint8_t* data, uint8_t dataLen, int8_t rssi);
typedef void (*BLEConnectedEventHandler)(ble_gap_addr_t* peerAddress);
typedef void (*BLEDisconnectedEventHandler)(uint8_t disconnectReason);
typedef void (*BLETimeoutEventHandler)(uint8_t timeoutSource);

#ifdef BLE_CENTRAL_ROLE_DEBUG
typedef void (*BLEDebugEventHandler)(int debugParameter, uint32_t errCode);
#endif // BLE_CENTRAL_ROLE_DEBUG

class BLECentralRole 
{
public:
	BLECentralRole();
	void setScanWindow(uint16_t window);
	void setScanInterval(uint16_t interval);
	void setScanTimeout(uint16_t timeout);
	void setScanActive(bool activeScan);

	void setConnMaxInterval(uint16_t maxInterval);
	void setConnMinInterval(uint16_t minInterval);
	void setConnSlaveLatency(uint16_t slaveLatency);
	void setConnSupTimeout(uint16_t connSupTimeout);

	bool begin();
	void poll(uint32_t* evtBuf, uint16_t* evtLen);
	uint32_t end();

	uint32_t startScan();
	uint32_t stopScan();

	uint32_t connect(ble_gap_addr_t* addr);
	uint32_t cancelConnection();
	uint32_t disconnect();

	void setScanEventCB(BLEScanEventHandler eventHandler);
	void setConnectedEventCB(BLEConnectedEventHandler eventHandler);
	void setDisconnectedEventCB(BLEDisconnectedEventHandler eventHandler);
	void setTimeoutEventCB(BLETimeoutEventHandler eventHandler);

protected:
	//void updateConnParameters();

private:
	uint16_t _connectionHandle;
	ble_gap_addr_t peerAddress;

	uint16_t _scanInterval;
	uint16_t _scanWindow;
	uint8_t _activeScan;
	uint16_t _scanTimeout;

	uint16_t _minConnInterval;
	uint16_t _maxConnInterval;
	uint16_t _slaveLatency;
	uint16_t _connSupTimeout;
	
	//BLECentralStates _centralState;

	BLEScanEventHandler _scanEventHandler;
	BLEConnectedEventHandler _connectedEventHandler;
	BLEDisconnectedEventHandler _disconnectedEventHandler;
	BLETimeoutEventHandler _timeoutEventHandler;

	ble_gap_scan_params_t _scanParams;
	ble_gap_conn_params_t _connParams;
};