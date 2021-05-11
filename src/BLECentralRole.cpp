#include "BLECentralRole.h"
#include "ble_hci.h"

BLECentralRole::BLECentralRole() :
	_connectionHandle(BLE_CONN_HANDLE_INVALID),
	_scanInterval(DEFAULT_SCAN_INTERVAL),
	_scanWindow(DEFAULT_SCAN_WINDOW),
	_activeScan(1),
	_scanTimeout(DEFAULT_SCAN_TIMEOUT),
	_minConnInterval(DEFAULT_MIN_CONN_INTERVAL),
	_maxConnInterval(DEFAULT_MAX_CONN_INTERVAL),
	_slaveLatency(DEFAULT_SLAVE_LATENCY),
	_connSupTimeout(DEFAULT_CONN_SUP_TIMEOUT)
{

	memset(&this->_scanParams, 0x00, sizeof(this->_scanParams));
	this->_scanParams.active = this->_activeScan;
	this->_scanParams.selective = 0;
	this->_scanParams.interval = this->_scanInterval;
	this->_scanParams.window = this->_scanWindow;
	this->_scanParams.timeout = this->_scanTimeout;
	this->_scanParams.p_whitelist = NULL;

	memset(&this->_connParams, 0x00, sizeof(this->_connParams));
	this->_connParams.max_conn_interval = this->_maxConnInterval;
	this->_connParams.min_conn_interval = this->_minConnInterval;
	this->_connParams.slave_latency = this->_slaveLatency;
	this->_connParams.conn_sup_timeout = this->_connSupTimeout;


}

void BLECentralRole::setScanWindow(uint16_t window)
{
	this->_scanWindow = window;
}

void BLECentralRole::setScanInterval(uint16_t interval)
{
	this->_scanInterval = interval;
}

void BLECentralRole::setScanTimeout(uint16_t timeout)
{
	this->_scanTimeout = timeout;
}

void BLECentralRole::setScanActive(bool activeScan)
{
	if (activeScan) { this->_activeScan = 1; }
	else { this->_activeScan = 0; }
}

void BLECentralRole::setConnMaxInterval(uint16_t maxInterval)
{
	this->_maxConnInterval = maxInterval;
}

void BLECentralRole::setConnMinInterval(uint16_t minInterval)
{
	this->_minConnInterval = minInterval;
}

void BLECentralRole::setConnSlaveLatency(uint16_t slaveLatency)
{
	this->_slaveLatency = slaveLatency;
}


void BLECentralRole::setConnSupTimeout(uint16_t connSupTimeout)
{
	this->_connSupTimeout = connSupTimeout;
}

bool BLECentralRole::begin()
{
	return true;
	//return this->startScan();
}

void BLECentralRole::poll(uint32_t* evtBuf, uint16_t* evtLen)
{
	ble_evt_t* bleEvt = (ble_evt_t*)evtBuf;

	switch (bleEvt->header.evt_id) {
	case BLE_GAP_EVT_ADV_REPORT: {
		ble_gap_evt_adv_report_t advReport = bleEvt->evt.gap_evt.params.adv_report;

		this->_scanEventHandler(&advReport.peer_addr, advReport.data, advReport.dlen, advReport.rssi);
		break;
	}

	case BLE_GAP_EVT_CONNECTED: {
		ble_gap_evt_connected_t connStruct = bleEvt->evt.gap_evt.params.connected;

		if (connStruct.role != BLE_GAP_ROLE_CENTRAL) {
			break;
		}

		this->peerAddress = connStruct.peer_addr;

		this->_connectionHandle = bleEvt->evt.gap_evt.conn_handle;

		this->_connectedEventHandler(&connStruct.peer_addr);

		sd_ble_gattc_primary_services_discover(this->_connectionHandle, 1, NULL);

		break;
	}

	case BLE_GAP_EVT_DISCONNECTED: {
		if (this->_connectionHandle != bleEvt->evt.gap_evt.conn_handle) {
			break;
		}

		this->_connectionHandle = BLE_CONN_HANDLE_INVALID;

		ble_gap_evt_disconnected_t disconnEvt = bleEvt->evt.gap_evt.params.disconnected;

		this->_disconnectedEventHandler(disconnEvt.reason);
		break;
	}

	case BLE_GAP_EVT_TIMEOUT: {
		uint8_t source = bleEvt->evt.gap_evt.params.timeout.src;

		this->_timeoutEventHandler(source);

		break;
	}

	case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST: {
		if (bleEvt->evt.gap_evt.conn_handle != this->_connectionHandle) {
			break;
		}

		ble_gap_conn_params_t updateParams = bleEvt->evt.gap_evt.params.conn_param_update_request.conn_params;

		uint8_t paramUpdateValid = 1;

		if (updateParams.max_conn_interval > BLE_GAP_CP_MAX_CONN_INTVL_MAX) {
			paramUpdateValid = 0;
		}

		if (updateParams.min_conn_interval < BLE_GAP_CP_MIN_CONN_INTVL_MIN) {
			paramUpdateValid = 0;
		}


		if (updateParams.conn_sup_timeout > BLE_GAP_CP_CONN_SUP_TIMEOUT_MAX || updateParams.conn_sup_timeout < BLE_GAP_CP_CONN_SUP_TIMEOUT_MIN) {
			paramUpdateValid = 0;
		}

		if (updateParams.slave_latency > BLE_GAP_CP_SLAVE_LATENCY_MAX) {
			paramUpdateValid = 0;
		}

		if (paramUpdateValid) {
			sd_ble_gap_conn_param_update(this->_connectionHandle, &updateParams);
		}
		else {
			sd_ble_gap_conn_param_update(this->_connectionHandle, NULL);
		}

		break;
	}

	default: {
		break;
	}
	}
}

uint32_t BLECentralRole::end()
{
	this->stopScan();
}

uint32_t BLECentralRole::startScan()
{
	memset(&this->_scanParams, 0x00, sizeof(ble_gap_scan_params_t));

	this->_scanParams.active = this->_activeScan;
	this->_scanParams.interval = this->_scanInterval;
	this->_scanParams.selective = 0;
	this->_scanParams.timeout = this->_scanTimeout;
	this->_scanParams.window = this->_scanWindow;

	uint32_t errCode = sd_ble_gap_scan_start(&this->_scanParams);

	return errCode;
}

uint32_t BLECentralRole::stopScan()
{
	uint32_t errCode = sd_ble_gap_scan_stop();

	return  errCode;
}

uint32_t BLECentralRole::connect(ble_gap_addr_t* addr)
{
	memset(&this->_scanParams, 0x00, sizeof(ble_gap_scan_params_t));

	this->_scanParams.active = _activeScan;
	this->_scanParams.interval = ((500*16)/10);
	this->_scanParams.selective = 0;
	this->_scanParams.timeout = this->_scanTimeout;
	this->_scanParams.window = ((200*16)/10);

	memset(&this->_connParams, 0x00, sizeof(ble_gap_conn_params_t));

	this->_connParams.max_conn_interval = this->_maxConnInterval;
	this->_connParams.min_conn_interval = this->_minConnInterval;
	this->_connParams.slave_latency = this->_slaveLatency;
	this->_connParams.conn_sup_timeout = this->_connSupTimeout;

	uint32_t errCode = sd_ble_gap_connect(addr, &this->_scanParams, &this->_connParams);

	return errCode;
}

uint32_t BLECentralRole::cancelConnection()
{
	return sd_ble_gap_connect_cancel();
}

uint32_t BLECentralRole::disconnect()
{
	return sd_ble_gap_disconnect(this->_connectionHandle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
}


void BLECentralRole::setScanEventCB(BLEScanEventHandler eventHandler)
{
	this->_scanEventHandler = eventHandler;
}

void BLECentralRole::setConnectedEventCB(BLEConnectedEventHandler eventHandler)
{
	this->_connectedEventHandler = eventHandler;
}

void BLECentralRole::setDisconnectedEventCB(BLEDisconnectedEventHandler eventHandler)
{
	this->_disconnectedEventHandler = eventHandler;
}

void BLECentralRole::setTimeoutEventCB(BLETimeoutEventHandler eventHandler)
{
	this->_timeoutEventHandler = eventHandler;
}