#include "BLECentralRole.h"
#include "ble_hci.h"


// todo why write requests don't work
// todo add characteristic notification/indicate support
// todo add local characteristics?

BLECentralRole::BLECentralRole() : _connectionHandle(BLE_CONN_HANDLE_INVALID),
								   _scanInterval(DEFAULT_SCAN_INTERVAL),
								   _scanWindow(DEFAULT_SCAN_WINDOW),
								   _activeScan(1),
								   _scanTimeout(DEFAULT_SCAN_TIMEOUT),
								   _minConnInterval(DEFAULT_MIN_CONN_INTERVAL),
								   _maxConnInterval(DEFAULT_MAX_CONN_INTERVAL),
								   _slaveLatency(DEFAULT_SLAVE_LATENCY),
								   _connSupTimeout(DEFAULT_CONN_SUP_TIMEOUT),
								   BLECentral(), // inherit BLECentral() so we can use already available characteristic event handlers
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

	init_attributes();
	init_callbacks();
}

void BLECentralRole::init_attributes()
{
	_remoteServiceDiscoveryIndex = 0;
	_numRemoteServices = _numRemoteCharacteristics = 0;
	attribute_discovery_complete = false;

	for(int i=0; i<10; ++i){
		_remoteServiceInfo[i].service = NULL;
		_remoteServiceInfo[i].handles_range = {0, 0};

		_remoteCharacteristicInfo[i].cccd_handle = 0;
		_remoteCharacteristicInfo[i].value_handle = 0;
		_remoteCharacteristicInfo[i].characteristic = NULL;
	}
}

void BLECentralRole::init_callbacks()
{
	_discovery_callbacks = {NULL, NULL, NULL};
	_connection_callbacks = {NULL, NULL, NULL, NULL, NULL};
	
	#if BLE_CENTRAL_ROLE_DEBUG
	debug_handler = NULL;
	#endif
}


// Scanning
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
	if (activeScan)
	{
		this->_activeScan = 1;
	}
	else
	{
		this->_activeScan = 0;
	}
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

	return errCode;
}


//Connection
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

uint32_t BLECentralRole::connect(ble_gap_addr_t *addr)
{
	memset(&this->_scanParams, 0x00, sizeof(ble_gap_scan_params_t));

	this->_scanParams.active = _activeScan;
	this->_scanParams.interval = ((500 * 16) / 10);
	this->_scanParams.selective = 0;
	this->_scanParams.timeout = this->_scanTimeout;
	this->_scanParams.window = ((200 * 16) / 10);
	this->_scanParams.p_whitelist = NULL;

	memset(&this->_connParams, 0x00, sizeof(ble_gap_conn_params_t));

	this->_connParams.max_conn_interval = this->_maxConnInterval;
	this->_connParams.min_conn_interval = this->_minConnInterval;
	this->_connParams.slave_latency = this->_slaveLatency;
	this->_connParams.conn_sup_timeout = this->_connSupTimeout;

	uint32_t errCode = sd_ble_gap_connect(addr, &this->_scanParams, &this->_connParams);

	return errCode;
}

uint32_t BLECentralRole::discoverServices()
{
	return sd_ble_gattc_primary_services_discover(this->_connectionHandle, 1, NULL);
}

uint32_t BLECentralRole::cancelConnection()
{
	return sd_ble_gap_connect_cancel();
}

uint32_t BLECentralRole::disconnect()
{
	return sd_ble_gap_disconnect(this->_connectionHandle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
}


//GATTC Methods
void BLECentralRole::addRemoteAttribute(BLERemoteAttribute& attribute)
{
	BLEUuid uuid = BLEUuid(attribute.uuid());
	const unsigned char *uuidData = uuid.data();

	ble_uuid_t nordicUUID;

	if (uuid.length() == 2)
	{
		nordicUUID.uuid = (uuidData[1] << 8) | uuidData[0];
		nordicUUID.type = BLE_UUID_TYPE_BLE;
	}
	else
	{
		unsigned char uuidDataTemp[16];

		memcpy(&uuidDataTemp, uuidData, sizeof(uuidDataTemp));

		nordicUUID.uuid = (uuidData[13] << 8) | uuidData[12];

		uuidDataTemp[13] = 0;
		uuidDataTemp[12] = 0;

		uint32_t res = sd_ble_uuid_vs_add((ble_uuid128_t *)&uuidDataTemp, &nordicUUID.type);
		#if BLE_CENTRAL_ROLE_DEBUG
		if(debug_handler != NULL){
			debug_handler(BLE_DEBUG_OP_VS_UUID_ADD, res, NULL);
		}
		#endif
	}

	if(attribute.type() == BLETypeService){
		_remoteServiceInfo[_numRemoteServices].service = (BLERemoteService *)&attribute;
		_remoteServiceInfo[_numRemoteServices].uuid = nordicUUID; 
		_remoteServiceInfo[_numRemoteServices].handles_range = {0, 0};
		_numRemoteServices++;
	}
	else if(attribute.type() == BLETypeCharacteristic){
		_remoteCharacteristicInfo[_numRemoteCharacteristics].characteristic = (BLERemoteCharacteristic *)&attribute;
		_remoteCharacteristicInfo[_numRemoteCharacteristics].uuid = nordicUUID; 
		_remoteCharacteristicInfo[_numRemoteCharacteristics].characteristic->setValueChangeListener((BLERemoteCharacteristicValueChangeListener&)*this);
		_numRemoteCharacteristics++;
	}
}


// Central Role methods
bool BLECentralRole::begin()
{
	return true;
}

// Main Loop
void BLECentralRole::gattc_loop(ble_evt_t *evt)
{
	switch(evt->header.evt_id){
		case BLE_GATTC_EVT_READ_RSP:{
			if(evt->evt.gattc_evt.conn_handle != this->_connectionHandle){
				break;
			}

			ble_gattc_evt_read_rsp_t read_resp = evt->evt.gattc_evt.params.read_rsp;

			for(int i=0; i<_numRemoteCharacteristics; ++i){
				if(_remoteCharacteristicInfo[i].value_handle == read_resp.handle || _remoteCharacteristicInfo[i].cccd_handle == read_resp.handle){
					_remoteCharacteristicInfo[i].characteristic->setValue((BLECentral &)*this, read_resp.data, read_resp.len);
					break;
				}
			}

			remote_request_in_progress = 0;

			break;
		}
		case BLE_GATTC_EVT_CHAR_VAL_BY_UUID_READ_RSP:{
			if(evt->evt.gattc_evt.conn_handle != this->_connectionHandle){
				break;
			}

			ble_gattc_evt_char_val_by_uuid_read_rsp_t resp = evt->evt.gattc_evt.params.char_val_by_uuid_read_rsp;

			for(int evt_index=0; evt_index < resp.count; ++evt_index){
				for(int i=0; i<_numRemoteCharacteristics; ++i){
					if(_remoteCharacteristicInfo[i].value_handle == resp.handle_value[evt_index].handle){
						_remoteCharacteristicInfo[i].characteristic->setValue((BLECentral &)*this, resp.handle_value[evt_index].p_value, resp.value_len);
						break;
					}
				}
			}

			remote_request_in_progress = 0;
			break;
		}
		case BLE_GATTC_EVT_WRITE_RSP:{
			if(evt->evt.gattc_evt.conn_handle != this->_connectionHandle){
				break;
			}

			ble_gattc_evt_write_rsp_t write_resp = evt->evt.gattc_evt.params.write_rsp;
			for(int i=0; i<_numRemoteCharacteristics; ++i){
				if(_remoteCharacteristicInfo[i].value_handle == write_resp.handle){
					_remoteCharacteristicInfo[i].characteristic->setValue((BLECentral &)*this, write_resp.data, write_resp.len);
					break;
				}
			}

			remote_request_in_progress = 0;
			break;
		}
		default:{
			break;
		}
	}
}

void BLECentralRole::poll(uint32_t *evtBuf, uint16_t *evtLen)
{
	ble_evt_t *bleEvt = (ble_evt_t *)evtBuf;

	gattc_loop(bleEvt);

	switch (bleEvt->header.evt_id){
	case BLE_EVT_TX_COMPLETE:{
		if(bleEvt->evt.common_evt.conn_handle != this->_connectionHandle){
			break;
		}

		ble_evt_tx_complete_t tx_resp = bleEvt->evt.common_evt.params.tx_complete;
		tx_buffer_count += tx_resp.count;
		remote_request_in_progress = 0;

		#if BLE_CENTRAL_ROLE_DEBUG

		for(int i=0; i<_numRemoteCharacteristics; ++i){
			if(last_op_handle == _remoteCharacteristicInfo[i].value_handle){
				_remoteCharacteristicInfo[i].characteristic->setValue((BLECentral &)*this, (uint8_t*)0xff, 1);
			}
		}

		if(debug_handler != NULL){
			debug_handler(BLE_DEBUG_OP_TX_COMPLETE, tx_buffer_count, NULL);
		}
		#endif
		break;
	}		
	case BLE_GAP_EVT_ADV_REPORT:{
		ble_gap_evt_adv_report_t advReport = bleEvt->evt.gap_evt.params.adv_report;

		_connection_callbacks._scanEventHandler(&advReport.peer_addr, advReport.data, advReport.dlen, advReport.rssi);
		break;
	}
	case BLE_GAP_EVT_CONNECTED:{
		ble_gap_evt_connected_t connStruct = bleEvt->evt.gap_evt.params.connected;

		if (connStruct.role != BLE_GAP_ROLE_CENTRAL)
		{
			break;
		}

		this->peerAddress = connStruct.peer_addr;

		this->_connectionHandle = bleEvt->evt.gap_evt.conn_handle;

		_connection_callbacks._connectedEventHandler(&connStruct.peer_addr);

		sd_ble_tx_packet_count_get(this->_connectionHandle, &tx_buffer_count);

		uint32_t res = sd_ble_gattc_primary_services_discover(this->_connectionHandle, 1, NULL);
		
		#if BLE_CENTRAL_ROLE_DEBUG
		if(debug_handler != NULL){
			debug_handler(BLE_DEBUG_OP_DISC_PRIM_SERVICES, res, NULL);
		}
		#endif

		break;
	}
	case BLE_GAP_EVT_DISCONNECTED:{
		if (this->_connectionHandle != bleEvt->evt.gap_evt.conn_handle)
		{
			break;
		}

		this->_connectionHandle = BLE_CONN_HANDLE_INVALID;
		this->tx_buffer_count = 0;
		this->remote_request_in_progress = 0;
		discovered_services = discovered_chr = 0;
		attribute_discovery_complete = false;

		ble_gap_evt_disconnected_t disconnEvt = bleEvt->evt.gap_evt.params.disconnected;

		_connection_callbacks._disconnectedEventHandler(disconnEvt.reason);
		break;
	}
	case BLE_GAP_EVT_TIMEOUT:{
		uint8_t source = bleEvt->evt.gap_evt.params.timeout.src;

		_connection_callbacks._timeoutEventHandler(source);

		break;
	}
	case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:{
		if (bleEvt->evt.gap_evt.conn_handle != this->_connectionHandle)
		{
			break;
		}

		sd_ble_gap_conn_param_update(this->_connectionHandle, &bleEvt->evt.gap_evt.params.conn_param_update_request.conn_params);

		break;
	}
	case BLE_GAP_EVT_CONN_PARAM_UPDATE:{
		if (bleEvt->evt.gap_evt.conn_handle != this->_connectionHandle){
			break;
		}

		if (_connection_callbacks._connParamUpdateHandler != NULL){
			_connection_callbacks._connParamUpdateHandler(&bleEvt->evt.gap_evt.params.conn_param_update.conn_params);
		}

		break;
	}
	case BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP:{
		if (bleEvt->evt.gattc_evt.conn_handle != this->_connectionHandle){
			break;
		}		

		if (bleEvt->evt.gattc_evt.gatt_status == BLE_GATT_STATUS_SUCCESS){
			this->on_services_discovered(&bleEvt->evt.gattc_evt.params.prim_srvc_disc_rsp);
		}
		else{
			uint32_t res = NRF_ERROR_NOT_FOUND;
			for(int i=0; i<_numRemoteServices; ++i){
				if(this->_remoteServiceInfo[i].handles_range.end_handle != 0 && this->_remoteServiceInfo[i].handles_range.start_handle != 0){
					res = sd_ble_gattc_characteristics_discover(this->_connectionHandle, &_remoteServiceInfo[i].handles_range);	
					_remoteServiceDiscoveryIndex = i;
					break;				
				}
			}

			// services discovered
			if (_discovery_callbacks.services_cb != NULL){
				_discovery_callbacks.services_cb(bleEvt->evt.gattc_evt.gatt_status, discovered_services, bleEvt->evt.gattc_evt.params.prim_srvc_disc_rsp.services);
			}			

			#if BLE_CENTRAL_ROLE_DEBUG	
			if(debug_handler != NULL){
				debug_handler(BLE_DEBUG_OP_DISC_CHR, res, NULL);
			}
			#endif
		}

		break;
	}
	case BLE_GATTC_EVT_CHAR_DISC_RSP:{
		if (bleEvt->evt.gattc_evt.conn_handle != this->_connectionHandle){
			break;
		}

		if (_discovery_callbacks.chr_cb != NULL)
		{
			_discovery_callbacks.chr_cb(bleEvt->evt.gattc_evt.gatt_status,  discovered_chr,
										bleEvt->evt.gattc_evt.params.char_disc_rsp.chars);
		}

		if (bleEvt->evt.gattc_evt.gatt_status == BLE_GATT_STATUS_SUCCESS){
			this->on_characteristics_discovered(&bleEvt->evt.gattc_evt.params.char_disc_rsp);
		}

		break;
	}
	default:{
		break;
	}
	}
}

uint32_t BLECentralRole::end()
{
	this->stopScan();
}


// Central Connection Callbacks
void BLECentralRole::setScanEventHandler(BLEScanEventHandler eventHandler)
{
	_connection_callbacks._scanEventHandler = eventHandler;
}

void BLECentralRole::setConnectedEventHandler(BLEConnectedEventHandler eventHandler)
{
	_connection_callbacks._connectedEventHandler = eventHandler;
}

void BLECentralRole::setDisconnectedEventHandler(BLEDisconnectedEventHandler eventHandler)
{
	_connection_callbacks._disconnectedEventHandler = eventHandler;
}

void BLECentralRole::setTimeoutEventHandler(BLETimeoutEventHandler eventHandler)
{
	_connection_callbacks._timeoutEventHandler = eventHandler;
}

void BLECentralRole::setConnectionParamUpdateEventHandler(BLEConnParamUpdateHandler eventHandler)
{
	_connection_callbacks._connParamUpdateHandler = eventHandler;
}


// GATTC Calllbacks
void BLECentralRole::setGattcServicesDiscoveredEventHandler(BLEServicesDiscoveredCB eventHandler)
{
	_discovery_callbacks.services_cb = eventHandler;
}

void BLECentralRole::setGattcCharacteristicsDiscoveredEventHandler(BLECharacteristicsDiscoveredCB eventHandler)
{
	_discovery_callbacks.chr_cb = eventHandler;
}

void BLECentralRole::setGattcDescriptorsDiscoveredEventHandler(BLEDescriptorsDiscoveredCB eventHandler)
{
	_discovery_callbacks.desc_cb = eventHandler;
}

void BLECentralRole::setGattcAttributeDiscoveryCompleteEventHandler(BLEAttrDiscoveryCompleteCB eventHandler)
{
	_discovery_callbacks.complete_cb = eventHandler;
}


// DEBUG
#if BLE_CENTRAL_ROLE_DEBUG
void BLECentralRole::setDebugHandler(BLEDebugEventHandler handler){
	this->debug_handler = handler;
}
#endif

// GATTC Discovery procedures
void BLECentralRole::on_services_discovered(ble_gattc_evt_prim_srvc_disc_rsp_t *resp)
{
	for (int i = 0; i < resp->count; ++i)
	{
		ble_gattc_service_t tempService = resp->services[i];

		for (int j = 0; j < _numRemoteServices; ++j)
		{
			if (_remoteServiceInfo[j].uuid.uuid == tempService.uuid.uuid && _remoteServiceInfo[j].uuid.type == tempService.uuid.type)
			{
				this->_remoteServiceInfo[j].handles_range = tempService.handle_range;
				discovered_services++;
				break;
			}
		}
	}

	uint16_t nextHandle = 0;
	if(resp->services[resp->count-1].handle_range.end_handle == 0xffff){
		nextHandle = resp->services[resp->count - 1].handle_range.end_handle;
	}
	else{
		nextHandle = resp->services[resp->count - 1].handle_range.end_handle + 1;
	}

	uint32_t res = sd_ble_gattc_primary_services_discover(this->_connectionHandle, nextHandle, NULL);

	#if BLE_CENTRAL_ROLE_DEBUG	
		if(debug_handler != NULL){
			debug_handler(BLE_DEBUG_OP_DISC_PRIM_SERVICES, res, NULL);
		}
	#endif	
}

void BLECentralRole::on_characteristics_discovered(ble_gattc_evt_char_disc_rsp_t *resp)
{
	uint8_t count = resp->count;

	for (int i = 0; i < count; ++i)
	{
		for (int j = 0; j < _numRemoteCharacteristics; ++j)
		{
			if (this->_remoteCharacteristicInfo[j].uuid.uuid == resp->chars[i].uuid.uuid && 
				this->_remoteCharacteristicInfo[j].uuid.type == resp->chars[i].uuid.type)
			{
				_remoteCharacteristicInfo[j].value_handle = resp->chars[i].handle_value;
				_remoteCharacteristicInfo[j].properties = resp->chars[i].char_props;	

				_remoteCharacteristicInfo[j].remoteService = &_remoteServiceInfo[_remoteServiceDiscoveryIndex];	

				_remoteCharacteristicInfo[j].cccd_handle = _remoteCharacteristicInfo[j].value_handle + 1;
				discovered_chr++;
				break;						
			}
		}
	}

	uint32_t res = NRF_ERROR_NOT_FOUND;
	bool remote_chr_available = false;

	for (int i=++_remoteServiceDiscoveryIndex; i<_numRemoteServices; ++i)
	{
		if (_remoteServiceInfo[i].handles_range.start_handle != 0 && _remoteServiceInfo[i].handles_range.end_handle != 0)
		{
			_remoteServiceDiscoveryIndex = i;
			remote_chr_available = true;
			break;
		}
	}

	if(!remote_chr_available ||
		_remoteServiceDiscoveryIndex >= _numRemoteServices){
			_discovery_callbacks.complete_cb();
	}
	else{
		res = sd_ble_gattc_characteristics_discover(this->_connectionHandle, &_remoteServiceInfo[_remoteServiceDiscoveryIndex].handles_range);
	}
		
	#if BLE_CENTRAL_ROLE_DEBUG	
		if(debug_handler != NULL){
			debug_handler(BLE_DEBUG_OP_DISC_CHR, _remoteServiceDiscoveryIndex, NULL);
		}
	#endif			
}


// Value update functions
bool BLECentralRole::canReadRemoteCharacteristic(BLERemoteCharacteristic& characteristic)
{
	for(int i=0; i<_numRemoteCharacteristics; ++i){
		if(_remoteCharacteristicInfo[i].value_handle != BLE_GATT_HANDLE_INVALID &&
			_remoteCharacteristicInfo[i].characteristic == &characteristic){
			return (remote_request_in_progress == 0 && _remoteCharacteristicInfo[i].properties.read);
		}
	}
	return false;
}

bool BLECentralRole::readRemoteCharacteristic(BLERemoteCharacteristic& characteristic)
{
	uint16_t offset = 0;
	bool op_success = false;
	uint32_t res = NRF_ERROR_NOT_FOUND;

	for(int i=0; i<_numRemoteCharacteristics; ++i){
		if(_remoteCharacteristicInfo[i].characteristic == &characteristic && _remoteCharacteristicInfo[i].value_handle != 0){
			res = sd_ble_gattc_read(this->_connectionHandle, _remoteCharacteristicInfo[i].value_handle, offset);
			if(res == NRF_SUCCESS){
				remote_request_in_progress = 1;
				op_success = true;
			}
			break;
		}
	}

	#if BLE_CENTRAL_ROLE_DEBUG
	if(debug_handler != NULL){
		debug_handler(BLE_DEBUG_OP_READ_CHR, res, NULL);
	}
	#endif

	return op_success;
}

bool BLECentralRole::canWriteRemoteCharacteristic(BLERemoteCharacteristic& characteristic)
{
	for(int i=0; i<_numRemoteCharacteristics; ++i){
		if(_remoteCharacteristicInfo[i].characteristic == &characteristic && _remoteCharacteristicInfo[i].value_handle != BLE_GATT_HANDLE_INVALID){
			if(_remoteCharacteristicInfo[i].properties.write){
				return (remote_request_in_progress == 0);
			}
			else if(_remoteCharacteristicInfo[i].properties.write_wo_resp){
				return (tx_buffer_count > 0);
			}
		} 
	}

	return false;
}

bool BLECentralRole::writeRemoteCharacteristic(BLERemoteCharacteristic& characteristic, const unsigned char value[], unsigned char length)
{
	bool op_success = false;
	uint16_t offset = 0;
	uint32_t res = NRF_ERROR_NOT_FOUND;

	ble_gattc_write_params_t writeParams;
	for(int i=0; i<_numRemoteCharacteristics; ++i){
		if(_remoteCharacteristicInfo[i].characteristic == &characteristic && _remoteCharacteristicInfo[i].value_handle != BLE_GATT_HANDLE_INVALID){
			writeParams.flags = 0;
			writeParams.handle = _remoteCharacteristicInfo[i].value_handle;
			writeParams.len = length;
			writeParams.offset = offset;
			writeParams.p_value = (uint8_t *)value;
			writeParams.write_op = (_remoteCharacteristicInfo[i].properties.write & 0x01) ? BLE_GATT_OP_WRITE_REQ : BLE_GATT_OP_WRITE_CMD;

			res = sd_ble_gattc_write(this->_connectionHandle, &writeParams);
			if(res == NRF_SUCCESS){
				remote_request_in_progress = 1;
				tx_buffer_count--;
				last_op_handle = writeParams.handle;
				op_success = true;
				break;
			}
		}
	}

	#if BLE_CENTRAL_ROLE_DEBUG
	if(debug_handler != NULL){
		debug_handler(BLE_DEBUG_OP_WRITE_CHR, writeParams.write_op, NULL);
	}
	#endif

	return op_success;
}

bool BLECentralRole::canSubscribeRemoteCharacteristic(BLERemoteCharacteristic& characteristic)
{
	for(int i=0; i<_numRemoteCharacteristics; ++i){
		if(_remoteCharacteristicInfo[i].characteristic == &characteristic){
			return (_remoteCharacteristicInfo[i].value_handle != BLE_GATT_HANDLE_INVALID && 
					(_remoteCharacteristicInfo[i].properties.notify || _remoteCharacteristicInfo[i].properties.indicate));
		}
	}

	return false;
}

bool BLECentralRole::subscribeRemoteCharacteristic(BLERemoteCharacteristic& characteristic)
{
	if(remote_request_in_progress || !this->canSubscribeRemoteCharacteristic(characteristic)){
		return false;
	}
	uint32_t res = NRF_ERROR_NOT_FOUND;
	bool op_success = false;

	for(int i=0; i<_numRemoteCharacteristics; ++i){
		if(_remoteCharacteristicInfo[i].characteristic == &characteristic && _remoteCharacteristicInfo[i].cccd_handle != BLE_GATT_HANDLE_INVALID){
			ble_gattc_write_params_t writeParams;
			uint8_t value[] = {(_remoteCharacteristicInfo[i].properties.notify) ? 0x01 : 0x02, 0x00};
			writeParams.flags = 0;
			writeParams.handle = _remoteCharacteristicInfo[i].cccd_handle;
			writeParams.len = sizeof(value);
			writeParams.offset = 0;
			writeParams.p_value = value;
			writeParams.write_op = BLE_GATT_OP_WRITE_CMD;

			res = sd_ble_gattc_write(this->_connectionHandle, &writeParams);
			if(res == NRF_SUCCESS){
				remote_request_in_progress = 1;
				op_success = true;
				break;
			}
		}
	}

	#if BLE_CENTRAL_ROLE_DEBUG
	if(debug_handler != NULL){
		debug_handler(BLE_DEBUG_OP_WRITE_CHR, res, NULL);
	}
	#endif

	return op_success;
}

bool BLECentralRole::canUnsubscribeRemoteCharacteristic(BLERemoteCharacteristic& characteristic)
{
	return this->canSubscribeRemoteCharacteristic(characteristic);
}

bool BLECentralRole::unsubcribeRemoteCharacteristic(BLERemoteCharacteristic& characteristic)
{
	bool op_success = false;
	uint32_t res = NRF_ERROR_NOT_FOUND;
	for(int i=0; i<_numRemoteCharacteristics; ++i){
		if(_remoteCharacteristicInfo[i].characteristic == &characteristic){
			ble_gattc_write_params_t writeParams;
			uint8_t value[] = {0x00, 0x00};
			writeParams.flags = 0;
			writeParams.handle = _remoteCharacteristicInfo[i].cccd_handle;
			writeParams.len = sizeof(value);
			writeParams.offset = 0;
			writeParams.p_value = value;
			writeParams.write_op = BLE_GATT_OP_WRITE_REQ;

			res = sd_ble_gattc_write(this->_connectionHandle, &writeParams);
			if(res == NRF_SUCCESS){
				remote_request_in_progress = 1;
				op_success = true;
				break;
			}
		}
	}

	#if BLE_CENTRAL_ROLE_DEBUG
	if(debug_handler != NULL){
		debug_handler(BLE_DEBUG_OP_WRITE_CHR, res, NULL);
	}
	#endif

	return op_success;
}