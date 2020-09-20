#ifndef W_NETWORK_H
#define W_NETWORK_H

#include <Arduino.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#ifdef ESP8266
#include <ESP8266mDNS.h>
#else
#include <ESPmDNS.h>
#endif
#include <WiFiUdp.h>
#include <DNSServer.h>
#include <StreamString.h>
#include "WHtmlPages.h"
#ifndef MINIMAL
#include "WAdapterMqtt.h"
#endif
#include "WStringStream.h"
#include "WDevice.h"
#include "WLed.h"
#include "WSettings.h"
#include "WJsonParser.h"
#include "WLog.h"
#include "webserverHelper.h"

#define SIZE_MQTT_PACKET 1536
#define SIZE_JSON_PACKET 3096
#define NO_LED -1
const char* CONFIG_PASSWORD PROGMEM = "12345678";
const char* APPLICATION_JSON PROGMEM = "application/json";
const char* TEXT_HTML PROGMEM = "text/html";
const char* TEXT_PLAIN PROGMEM = "text/plain";

const char* DEFAULT_TOPIC_STATE PROGMEM = "properties";
const char* DEFAULT_TOPIC_SET PROGMEM = "set";

#ifndef MINIMAL
const char* MQTT_CMND PROGMEM = "cmnd";
const char* MQTT_STAT PROGMEM = "stat";
const char* MQTT_TELE PROGMEM = "tele";
#endif

const char* FORM_PW_NOCHANGE PROGMEM = "___NOCHANGE___";

const byte NETBITS1_MQTT       = 1;
const byte NETBITS1_HASS       = 2;
const byte NETBITS1_APFALLBACK = 4;
const byte NETBITS1_MQTTSINGLEVALUES = 8;

const char* PROP_IDX PROGMEM = "idx";
const char* PROP_SSID PROGMEM = "ssid";
const char* PROP_PASSWORD PROGMEM = "password";
const char* PROP_NETBITS1 PROGMEM = "netbits1";
const char* PROP_MQTTSERVER PROGMEM = "mqttServer";
const char* PROP_MQTTPORT PROGMEM = "mqttPort";
const char* PROP_MQTTUSER PROGMEM = "mqttUser";
const char* PROP_MQTTPASSWORD PROGMEM = "mqttPassword";
const char* PROP_MQTTTOPIC PROGMEM = "mqttTopic";

#ifdef DEBUG
const int maxApRunTimeMinutes = 2;
const int maxConnectFail = 3;
#else
const int maxApRunTimeMinutes = 5;
const int maxConnectFail = 10;
#endif

typedef enum wnWifiMode
{
	WIFIMODE_UNSET = 0,
	WIFIMODE_STATION = 1,
	WIFIMODE_AP = 2,
	WIFIMODE_FALLBACK = 3
} wnWifiMode_t;



WiFiEventHandler gotIpEventHandler, disconnectedEventHandler;
WiFiClient wifiClient;
#ifndef MINIMAL
WAdapterMqtt *mqttClient;
#endif

class WNetwork {
public:
	typedef std::function<void(void)> THandlerFunction;
	typedef std::function<bool(void)> THandlerReturnFunction;
	typedef std::function<bool(bool)> THandlerReturnFunctionBool;
	WNetwork(bool debug, String applicationName, String firmwareVersion,
			int statusLedPin, byte appSettingsFlag) {

		this->onConfigurationFinished = nullptr;
		this->onNotify = nullptr;
#ifndef MINIMAL
		this->onMqttHassAutodiscover = nullptr;
#endif
		/* https://github.com/esp8266/Arduino/blob/master/doc/esp8266wifi/station-class.rst#reconnect */
		//Serial.printf("AC: %d, ARC: %d\n", WiFi.getAutoConnect(), WiFi.getAutoReconnect());
		WiFi.disconnect();
		WiFi.mode(WiFiMode::WIFI_STA);
		WiFi.encryptionType(wl_enc_type::ENC_TYPE_CCMP);
		WiFi.setOutputPower(20.5);
		WiFi.setPhyMode(WiFiPhyMode::WIFI_PHY_MODE_11N);
		WiFi.setAutoConnect(false);
		WiFi.setAutoReconnect(true);
		WiFi.persistent(false);
		this->applicationName = applicationName;
		this->firmwareVersion = firmwareVersion;
		this->webServer = nullptr;
		this->dnsApServer = nullptr;
		this->debug = debug;
		this->statusLedPin = statusLedPin;
		wlog = new WLog((this->debug ? LOG_LEVEL_TRACE : LOG_LEVEL_SILENT), LOG_LEVEL_SILENT, &Serial);
		wlog->setOnLogCommand([this](int level, const char * message) {
			if (this->networkLogActive) return;
			this->networkLogActive=true; /* avoid endless loop */
			WDevice *device = this->firstDevice;
			while (device != nullptr) {
				device->sendLog(level, message);
				device = device->next;
			}
			this->networkLogActive=false;
		});

		this->updateRunning = false;
		this->restartFlag = nullptr;
		this->networkLogActive = false;
		this->connectFailCounter = 0;
		this->apStartedAt = 0;
		this->lastLoopLog = 0;
#ifndef MINIMAL
		this->lastMqttHassAutodiscoverySent = 0;
		this->mqttClient = nullptr;
#endif
		//this->webSocket = nullptr;
		settings = new WSettings(wlog, appSettingsFlag, false);
		if (settings->getNetworkSettingsVersion()!=NETWORKSETTINGS_CURRENT){
			wlog->trace(F("loading oldSettings (network: %d)"), settings->getNetworkSettingsVersion());
			settingsOld = new WSettings(wlog, appSettingsFlag, true);
		} else {
			settingsOld = nullptr;
		}
		settingsFound = loadSettings();
#ifndef MINIMAL
		lastMqttConnect = lastWifiConnect = 0;
#endif
		this->wifiModeDesired = (settingsFound &&  getSsid() != "" ? wnWifiMode::WIFIMODE_STATION : wnWifiMode::WIFIMODE_AP);
		this->wifiModeRunning = wnWifiMode::WIFIMODE_UNSET;
		lastWifiStatus = -1;


		gotIpEventHandler = WiFi.onStationModeGotIP(
				[this](const WiFiEventStationModeGotIP &event) {
					wlog->notice(F("WiFi: Station connected, IP: %s"), this->getDeviceIp().toString().c_str());
					this->connectFailCounter=0;
					startMDNS();
					//Connect, if webThing supported and Wifi is connected as client
					this->notify(true);
				});
		disconnectedEventHandler = WiFi.onStationModeDisconnected(
				[this](const WiFiEventStationModeDisconnected &event) {
					if (!isSoftAP()){
						this->connectFailCounter++;
						wlog->notice(F("WiFi: Station disconnected (count: %d)"), this->connectFailCounter);
#ifndef MINIMAL
						this->disconnectMqtt();
						this->lastMqttConnect = 0;
#endif
					}
				});

		
		wlog->notice(F("firmware: %s"), firmwareVersion.c_str());

	}


	//returns true, if no configuration mode and no own ap is opened
	bool loop(unsigned long now) {
		
#ifdef DEBUG
		if (this->lastLoopLog == 0 || now  > this->lastLoopLog + 2000){
			if (WiFi.status() != WL_CONNECTED) wlog->trace(F("WiFi: loop, wifiStatus: %d, modeRunning: %d, modeDesired: %d, now: %d"),
			WiFi.status(), wifiModeRunning, wifiModeDesired, now);
			this->lastLoopLog=now;
		} 
#endif
		bool result = true;
		if ((
#ifdef MINIMAL
true ||
#endif
			this->isSupportingApFallback()) && wifiModeRunning== wnWifiMode::WIFIMODE_STATION && this->connectFailCounter >= maxConnectFail && !isUpdateRunning()){
			wlog->warning(F("WiFi: connectFailCounter > %d, activating AP"), maxConnectFail);
			wifiModeDesired = wnWifiMode::WIFIMODE_FALLBACK;
		}
		if (wifiModeRunning == wnWifiMode::WIFIMODE_FALLBACK && !isUpdateRunning()){
			// Switch back to Station Mode if config is available
			if (settingsFound && (now -  this->apStartedAt > maxApRunTimeMinutes * 60 * 1000)){
				wlog->warning(F("WiFi: AP ran %d minutes, trying now to use configured WLAN again, Wifi Status %d "), maxApRunTimeMinutes, WiFi.status());
				//this->stopWebServer();
				this->connectFailCounter=0;
				wifiModeDesired = wnWifiMode::WIFIMODE_STATION;
			}
		}
		if (wifiModeDesired!=wifiModeRunning){
			if (wifiModeRunning == wnWifiMode::WIFIMODE_FALLBACK || wifiModeRunning == wnWifiMode::WIFIMODE_AP){
				wlog->warning(F("WiFi: deactivating softApMode"));
#ifndef MINIMAL
				this->disconnectMqtt();
				this->lastMqttConnect = 0;
#endif
				WiFi.softAPdisconnect();
				lastWifiConnect=0;
				apStartedAt=0;
				deleteDnsApServer();

			}
			if (wifiModeRunning == wnWifiMode::WIFIMODE_STATION){
				WiFi.disconnect();
			}

			if (wifiModeDesired == wnWifiMode::WIFIMODE_FALLBACK || wifiModeDesired == wnWifiMode::WIFIMODE_AP){
				wlog->trace(F("Starting AP Mode"));
				wifiModeRunning = wifiModeDesired;
				// even if currently disconnected - stop trying to connect
				WiFi.disconnect();
				//Create own AP
				this->apStartedAt=millis();
				String apSsid = getClientName(false);
				wlog->notice(F("Start AccessPoint for configuration. SSID '%s'; password '%s'"), apSsid.c_str(), CONFIG_PASSWORD);
				dnsApServer = new DNSServer();
				WiFi.mode(WIFI_AP);
				WiFi.softAP(apSsid.c_str(), CONFIG_PASSWORD);
				dnsApServer->setErrorReplyCode(DNSReplyCode::NoError);
				dnsApServer->start(53, "*", WiFi.softAPIP());
			}


			
			if (wifiModeDesired == wnWifiMode::WIFIMODE_STATION){
				wlog->trace(F("Starting Station Mode"));
				wifiModeRunning = wifiModeDesired;
				lastWifiConnect = 0;
			}
		}
		if (wifiModeRunning == wnWifiMode::WIFIMODE_STATION && 
			WiFi.status() != WL_CONNECTED	&& (lastWifiConnect == 0 || now - lastWifiConnect > 20 * 1000)){
			wlog->notice(F("WiFi: Connecting to '%s', using Hostname '%s'"), getSsid(), getHostName().c_str());
			wlog->notice(F("WiFi: SSID/PSK/Hostname '%s'/'%s'/'%s' (strlen %d/%d/%d)"), getSsid(), getPassword(), getHostName().c_str(),
				strlen(getSsid()), strlen(getPassword()), strlen(getHostName().c_str()));
			WiFi.mode(WIFI_STA);
			WiFi.hostname(getHostName());
			WiFi.begin(getSsid(), getPassword());
			apStartedAt=0;
			lastWifiConnect = now;
		}



		if (isWebServerRunning()){
			if (isSoftAP()) {
				dnsApServer->processNextRequest();
			}
			result = ((!isSoftAP()) && (!isUpdateRunning()));
		}
		if (!isSoftAP()){
#ifndef MINIMAL
			//MQTT connection
			if ((lastMqttConnect == 0) || (now - lastMqttConnect > 300000)){
				if (mqttReconnect()) lastMqttConnect = now;
			}
			if ((!isUpdateRunning()) && (this->isMqttConnected())) {
				mqttClient->loop();
			}
#endif
		}

		//Loop led
		if (statusLed != nullptr) {
			statusLed->loop(now);
		}
		//Loop Devices
		WDevice *device = firstDevice;
		while (device != nullptr) {
			device->loop(now);
#ifndef MINIMAL
			if ((this->isMqttConnected()) && (this->isSupportingMqtt())
					&& ((device->lastStateNotify == 0)
							|| ((device->stateNotifyInterval > 0) && (now > device->lastStateNotify) &&
								(now - device->lastStateNotify > device->stateNotifyInterval)))
					&& (device->isDeviceStateComplete())) {
				wlog->notice(F("Notify interval is up -> Device state changed..."));
				handleDeviceStateChange(device);
			}
#endif
			device = device->next;
		}
#ifndef MINIMAL
		//WebThingAdapter
		if ((!isUpdateRunning()) && (this->isSupportingWebThing()) && (isWifiConnected())) {
			MDNS.update();
		}

		// process changed properties and send to MQTT
		handleDevicesChangedPropertiesMQTT();
#endif

		//Restart required?
		if (restartFlag!=nullptr) {
			wlog->notice(F("Restart flag: '%s'"), restartFlag);
			this->updateRunning = false;
			stopWebServer();
			delay(1000);
			ESP.restart();
			delay(2000);
		}
#ifndef MINIMAL
		if (lastMqttHassAutodiscoverySent==0){
				if (sendMqttHassAutodiscover(false) ) lastMqttHassAutodiscoverySent=now;
		}
#endif
		if (WiFi.status() != lastWifiStatus){
			wlog->notice(F("WiFi: Status changed from %d to %d"), lastWifiConnect, WiFi.status());
			lastWifiStatus = WiFi.status(); 
			this->notify( (WiFi.status() == wl_status_t::WL_CONNECTED) );
		}
		return result;
	}

	~WNetwork() {
		delete wlog;
	}

	WSettings* getSettings() {
		return this->settings;
	}
	WSettings* getSettingsOld() {
		return this->settingsOld;
	}
	void deleteSettingsOld(){
		if (this->settingsOld) delete this->settingsOld;
		this->settingsOld=nullptr;
	}

	void setOnNotify(THandlerFunction onNotify) {
		this->onNotify = onNotify;
	}

	void setOnConfigurationFinished(THandlerFunction onConfigurationFinished) {
		this->onConfigurationFinished = onConfigurationFinished;
	}

#ifndef MINIMAL
	bool sendMqttHassAutodiscover(bool removeDiscovery){
		if (isSupportingMqtt() && isSupportingMqttHASS() && isMqttConnected() && isStation() && onMqttHassAutodiscover){
			return onMqttHassAutodiscover(removeDiscovery);
		}
	}

	void setOnMqttHassAutodiscover(THandlerReturnFunctionBool onMqttHassAutodiscover) {
		this->onMqttHassAutodiscover = onMqttHassAutodiscover;
	}

	bool publishMqtt(const char* topic, const char * message, bool retained=false) {
		wlog->verbose(F("MQTT... '%s'"), topic);
		wlog->verbose(F("MQTT  .. '%s'"), message);
		if (isMqttConnected()) {
			wlog->verbose(F("MQTT connected... "));
			if (mqttClient->publish(topic, message, retained)) {
				wlog->verbose(F("MQTT sent. Topic: '%s'"), topic);
				return true;
			} else {
				wlog->verbose(F("Sending MQTT message failed, rc=%d"), mqttClient->state());
				this->disconnectMqtt();
				return false;
			}
		} else {
			wlog->notice(F("MQTT not connected... "));
			if (strcmp(getMqttServer(), "") != 0) {
				wlog->verbose(F("Can't send MQTT. Not connected to server: %s"), getMqttServer());
			}
			return false;
		}
		wlog->warning(F("publish MQTT mystery... "));
	}

	bool publishMqtt(const char* topic, WStringStream* response, bool retained=false) {
		return publishMqtt(topic, response->c_str(), retained);
	}

	bool publishMqtt(const char* topic, const char* key, const char* value, const char* key2=nullptr, const char* value2=nullptr, bool retained=false) {
		if (this->isSupportingMqtt()) {
			WStringStream* response = getResponseStream();
			WJson json(response);
			json.beginObject();
			json.propertyString(key, value);
			if (key2!=nullptr && value2!=nullptr){
				json.propertyString(key2, value2);
			}
			json.endObject();
			return publishMqtt(topic, response, retained);
		} else {
			return false;
		}
	}
#endif

	// Creates a web server
	void startWebServer() {
		wlog->trace(F("Starting Webserver"));
#ifndef MINIMAL
		if (this->isSupportingMqtt()) {
			this->mqttClient = new WAdapterMqtt(debug, wifiClient, SIZE_JSON_PACKET);
			mqttClient->setCallback(std::bind(&WNetwork::mqttCallback, this,
										std::placeholders::_1, std::placeholders::_2,
										std::placeholders::_3));
		}
#endif		
		if (this->statusLedPin != NO_LED) {
			statusLed = new WLed(statusLedPin);
			statusLed->setOn(true, 500);
		} else {
			statusLed = nullptr;
		}

		this->webServer = new AsyncWebServer(80);

		webServer->onNotFound([this](AsyncWebServerRequest *request){
			handleUnknown(request);
		});
		//if ((WiFi.status() != WL_CONNECTED) || (!this->isSupportingWebThing())) {

		// every webServer->on requests 144 Byte of Heap - so save heap
		webServer->on("", HTTP_GET, [this](AsyncWebServerRequest *request){
			String url=request->url();
			wlog->notice(F("Serving: %s"), url.c_str());
			if (url.equals("") || 
			url.equals(F("/")) || 
			// Android Captive Portal Detection
			url.equals(F("/generate_204")) || 
			// Apple Captive Portal Detection
			url.equals(F("/hotspot-detect.html"))){
				request->redirect(F("/config"));
			} else if (url.equals(F("/config")) || url.equals(F("/config/"))){
				handleHttpRootRequest(request);
			} else if (url.equals(F("/wifi"))){
				handleHttpNetworkConfiguration(request);
			} else if (url.equals(F("/save"))){
				handleHttpsave(request);
			} else if (url.equals(F("/info"))){
				handleHttpInfo(request);
			} else if (url.equals(F("/reset"))){
				handleHttpReset(request);
			} else if (url.equals(F("/firmware"))){
				handleHttpFirmwareUpdate(request);
			} else if (url.equals(F("/css"))){
				request->send(200, F("text/css"), PAGE_CSS);
			} else if (url.equals(F("/js"))){
				request->send(200, F("text/javascript"), PAGE_JS);
			} else {
				bool handled=false;
				WDevice *device = this->firstDevice;
				while (device != nullptr) {
					String did("/");
					did.concat(device->getId());
					String saveDid(F("/save_"));
					saveDid.concat(device->getId());
					if (url.equals(did)){
						handleHttpDeviceConfiguration(request, device);
						handled=true;
						break;
					} else if (url.equals(saveDid)){
						handleHttpSaveDeviceConfiguration(request, device);
						handled=true;
						break;
					}
					if (url.startsWith(did) or url.startsWith(saveDid)){
						WPage *subpage = device->firstPage;
						while (subpage != nullptr) {
							String didSub(did);
							didSub.concat("_");
							didSub.concat(subpage->getId());
							String saveDidSub(F("/save_"));
							saveDidSub.concat(did);
							saveDidSub.concat("_");
							saveDidSub.concat(subpage->getId());
							Serial.printf("X: %s   %s\n", did.c_str(), saveDid.c_str());
							if (url.equals(didSub)){
								Serial.printf("MATCH\n");
								handleHttpDevicePage(request, device, subpage);
								handled=true;
								break;
							}
							if (url.equals(saveDidSub)){
								Serial.printf("MATCH2\n");
								handleHttpDevicePageSubmitted(request, device, subpage);
								handled=true;
								break;
							}
							subpage = subpage->next;
						}
						if (handled) break; // why has c no break(2)
					}
					device = device->next;
				}
				if (!handled){
					handleUnknown(request);
				}
			}
		});
		webServer->on("/firmware", HTTP_POST, [this](AsyncWebServerRequest *request){
			handleHttpFirmwareUpdateFinished(request);
		}, [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
			handleHttpFirmwareUpdateProgress(request, filename, index, data, len, final);
		});
		webServer->on("/ws", HTTP_GET, [this](AsyncWebServerRequest *request){
				std::bind(&WNetwork::handleWebSocket, this);
		});

		wlog->notice(F("webServer prepared."));

		webServer->begin();
		this->notify(false);
		return;
	}

	void startMDNS(){
		#ifndef MINIMAL
				//WebThings
				if (this->isSupportingWebThing()) {
					//Make the thing discoverable
					//String mdnsName = getHostName() + ".local";
					String mdnsName = this->getDeviceIp().toString();
					wlog->notice(F("MDNS init, name: %s"), mdnsName.c_str());
					MDNS.end();
					if (MDNS.begin(mdnsName)) {
						wlog->notice(F("MDNS OK"));
						MDNS.addService("http", "tcp", 80);
						MDNS.addServiceTxt("http", "tcp", "url", "http://" + mdnsName + "/things");
						MDNS.addServiceTxt("http", "tcp", "webthing", "true");
						wlog->notice(F("MDNS responder started at %s"), mdnsName.c_str());
					}
					webServer->on("/things", HTTP_GET, [this](AsyncWebServerRequest *request){
						sendDevicesStructure(request);
					});
					webServer->on("/things/", HTTP_GET, [this](AsyncWebServerRequest *request){
						sendDevicesStructure(request);
					});
					WDevice *device = this->firstDevice;
					while (device != nullptr) {
						bindWebServerCallsNetwork(device);
						device = device->next;
					}
				}
		#endif
	}

	void stopWebServer() {
		if (this->updateRunning) return;
		if ((isWebServerRunning())) {
			wlog->notice(F("stopWebServer"));
			delay(100);
			webServer->end();
			this->notify(false);
		}
		deleteDnsApServer();
#ifndef MINIMAL
		disconnectMqtt();
#endif
		wlog->notice(F("kill"));
		if (webServer!=nullptr){
			delete webServer;
			webServer = nullptr;
		}
		if (onConfigurationFinished) {
			onConfigurationFinished();
		}
	}

	bool isWebServerRunning() {
		return (webServer != nullptr);
	}

	bool isUpdateRunning() {
		return this->updateRunning;
	}

	bool isDebug() {
		return this->debug;
	}

	bool isSoftAP() {
		return (wifiModeRunning == wnWifiMode::WIFIMODE_AP || wifiModeRunning == wnWifiMode::WIFIMODE_FALLBACK);
	}

	bool isSoftAPDesired() {
		return (wifiModeDesired == wnWifiMode::WIFIMODE_AP || wifiModeDesired == wnWifiMode::WIFIMODE_FALLBACK);
	}

	bool isStation() {
		return (wifiModeRunning == wnWifiMode::WIFIMODE_STATION);
	}

	bool isWifiConnected() {
		return ((!isSoftAP()) && (!isUpdateRunning())
				&& (WiFi.status() == WL_CONNECTED));
	}

#ifndef MINIMAL
	bool isMqttConnected() {
		return ((this->isSupportingMqtt()) && (this->mqttClient != nullptr)
				&& (this->mqttClient->connected()));
	}

	void disconnectMqtt() {
		if (this->mqttClient != nullptr) {
			this->mqttClient->disconnect();
		}
	}
#endif

	void deleteDnsApServer(){
		if (this->dnsApServer){
			this->dnsApServer->stop();
			delete dnsApServer;
			this->dnsApServer = nullptr;
		}
	}

	IPAddress getDeviceIp() {
		return (isSoftAP() ? WiFi.softAPIP() : WiFi.localIP());
	}

	bool isSupportingWebThing() {
		return true;
	}

	bool isSupportingMqtt() {
		return this->supportingMqtt->getBoolean();
	}

	bool isSupportingMqttHASS() {
		return this->supportingMqttHASS->getBoolean();
	}

	bool isSupportingApFallback() {
		return this->supportingApFallback->getBoolean();
	}

	bool isSupportingMqttSingleValues() {
		return this->supportingMqttSingleValues->getBoolean();
	}

	const char* getIdx() {
		return this->idx->c_str();
	}

	const char* getSsid() {
		return this->ssid->c_str();
	}

	const char* getPassword() {
		return settings->getString(PROP_PASSWORD);
	}

	const char* getMqttServer() {
		return settings->getString(PROP_MQTTSERVER);
	}

	const char* getMqttPort() {
		return settings->getString(PROP_MQTTPORT);
	}

	const char* getMqttTopic() {
		return this->mqttBaseTopic->c_str();
	}

	const char* getMqttUser() {
		return settings->getString(PROP_MQTTUSER);
	}

	const char* getMqttPassword() {
		return settings->getString(PROP_MQTTPASSWORD);
	}

	void addDevice(WDevice *device) {
		if (statusLed == nullptr) {
			statusLed = device->getStatusLed();
			if (statusLed != nullptr) {
				statusLed->setOn(true, 500);
			}
		}
		if (this->lastDevice == nullptr) {
			this->firstDevice = device;
			this->lastDevice = device;
		} else {
			this->lastDevice->next = device;
			this->lastDevice = device;
		}

		/*ToDo
		AsyncWebSocket *webSocket = new AsyncWebSocket("/things/" + device->getId());
		device->setWebSocket(webSocket);
		*/
	}

	WLog* log() {
		return wlog;
	}

	WStringStream* getResponseStream() {
		if (responseStream == nullptr) {
			responseStream = new WStringStream(SIZE_JSON_PACKET);
		}
		responseStream->flush();
		return responseStream;
	}
	void setDesiredModeAp(){
		this->wifiModeDesired = wnWifiMode::WIFIMODE_AP;
	}

	void setDesiredModeFallback(){
		this->wifiModeDesired = wnWifiMode::WIFIMODE_FALLBACK;
	}
	void setDesiredModeStation(){
		if (getSsid() != "" ) this->wifiModeDesired = wnWifiMode::WIFIMODE_STATION;
	}
	String getMacAddress(){
		return WiFi.macAddress();
	}
	String getApplicationName(){
		return applicationName;
	}

	String getFirmwareVersion(){
		return firmwareVersion;
	}


private:
	AsyncWebServer *webServer;
	WDevice *firstDevice = nullptr;
	WDevice *lastDevice = nullptr;
	WLog* wlog;
	THandlerFunction onNotify;
	THandlerFunction onConfigurationFinished;
	THandlerReturnFunctionBool onMqttHassAutodiscover;
	bool debug, updateRunning;
	const __FlashStringHelper* restartFlag;
	DNSServer *dnsApServer;
	int networkState;
	String applicationName;
	String firmwareVersion;
	const __FlashStringHelper* firmwareUpdateError;
	WProperty *netBits1;
	WProperty *supportingMqtt;
	WProperty *supportingMqttHASS;
	WProperty *supportingApFallback;
	WProperty *supportingMqttSingleValues;
	WProperty *mqttBaseTopic;
	WProperty *mqttStateTopic;
	WProperty *mqttSetTopic;
#ifndef MINIMAL
	WAdapterMqtt *mqttClient;
	long lastMqttConnect;
	unsigned long lastMqttHassAutodiscoverySent;
#endif
	WProperty *ssid;
	WProperty *idx;
	long lastWifiConnect;
	WStringStream* responseStream = nullptr;
	WLed *statusLed;
	int statusLedPin;
	WSettings *settings;
	WSettings *settingsOld;
	bool settingsFound;
	bool networkLogActive;
	int connectFailCounter;
	unsigned long apStartedAt;
	unsigned long lastLoopLog;

	wnWifiMode_t wifiModeDesired;
	wnWifiMode_t wifiModeRunning;
	int lastWifiStatus;

#ifndef MINIMAL
	void handleDeviceStateChange(WDevice *device) {
		String topic = String(getMqttTopic()) + "/" + MQTT_STAT + "/things/" + String(device->getId()) + "/properties";
		wlog->notice(F("Device state changed -> send device state... %s"), topic.c_str());
		mqttSendDeviceState(topic, device);
	}

	void mqttSendDeviceState(String topic, WDevice *device) {		
		if ((this->isMqttConnected()) && (isSupportingMqtt())){
			if (device->isDeviceStateComplete()) {
				wlog->notice(F("Send actual device state via MQTT %s"), topic.c_str());
				WStringStream* response = getResponseStream();
				WJson json(response);
				json.beginObject();
				if (device->isMainDevice()) {
					json.propertyString(PROP_IDX, getIdx());
					json.propertyString("ip", getDeviceIp().toString().c_str());
					json.propertyString("firmware", firmwareVersion.c_str());
				}
				device->toJsonValues(&json, MQTT);
				json.endObject();
								
				if (mqttClient->publish(topic.c_str(), response->c_str(), device->isMqttRetain())) {
					wlog->verbose(F("MQTT sent"));
				}
				device->lastStateNotify = millis();
			} else {
				wlog->warning(F("Not sending state via MQTT %s, deviceStateComplete=false"), topic.c_str());
			}
		}
	}

	void mqttCallback(char *ptopic, char *payload, unsigned int length) {
		String ptopicS=String(ptopic);
		String payloadS=String(payload);
		wlog->trace(F("Received MQTT callback: '%s'->'%s'"), ptopic, payload, strlen(ptopic), ptopicS.length());
		if (!ptopicS.startsWith(getMqttTopic())){
			wlog->notice(F("Ignoring, starts not with our topic '%s'"), getMqttTopic());
			return;
		}
		String fulltopic = ptopicS.substring(strlen(getMqttTopic()) + 1);
		String cmd = String(MQTT_CMND);
		if (fulltopic.startsWith(cmd)) {			
			String topic = String(fulltopic).substring(strlen(cmd.c_str()) + 1);
			if (topic.startsWith("things/")) {
				topic = topic.substring(String("things/").length());
				int i = topic.indexOf("/");
				if (i > -1) {
					String deviceId = topic.substring(0, i);					
					wlog->trace(F("look for device id '%s'"), deviceId.c_str());
					WDevice *device = this->getDeviceById(deviceId.c_str());
					if (device != nullptr) {
						String stat_topic = getMqttTopic() + (String)"/" + String(MQTT_STAT) + (String)"/things/" + String(deviceId) + (String)"/";
						topic = topic.substring(i + 1);	
						if (topic.startsWith("properties")) {							
							topic = topic.substring(String("properties").length() + 1);
							if (topic.equals("")) {
								if (length > 0) {
									//Check, if it's only response to a state before
									wlog->notice(F("Set several properties for device %s"), device->getId());
									WJsonParser* parser = new WJsonParser();
									if (parser->parse(payload, device) == nullptr) {
										wlog->warning(F("No properties updated for device %s"), device->getId());
									} else {
										wlog->trace(F("One or more properties updated for device %s"), device->getId());
									}
									delete parser;
								} else {
									wlog->notice(F("Empty payload for topic 'properties' -> send device state..."));
									//Empty payload for topic 'properties' ->  just send state (below)
								}


							} else {
								//There are still some more topics after properties
								//Try to find property with that topic and set single value
								WProperty* property = device->getPropertyById(topic.c_str());
								if ((property != nullptr) && (property->isVisible(MQTT))) {
									//Set Property
									wlog->notice(F("Set property '%s' for device %s"), property->getId(), device->getId(), payloadS.c_str());
									if (!property->parse(payloadS)) {
										wlog->warning(F("Property not updated."));
									} else {
										wlog->trace(F("Property updated."));
										// set to unchanged, because we're sending update now immediately
										property->setUnChanged();
									}
									// answer just with changed value
									publishMqtt((stat_topic+topic).c_str(), property->toString().c_str(), device->isMqttRetain());
								}
							}			
							wlog->notice(F("Sending device State to %sproperties for device %s"), stat_topic.c_str(), device->getName());				
							mqttSendDeviceState(stat_topic+"properties", device);
						} else {
							//unknown, ask the device
							device->handleUnknownMqttCallback(stat_topic, topic, payloadS, length);
						}
					}
				}
			}
		}
	}

	bool mqttReconnect() {
		if (this->isSupportingMqtt() && this->isWifiConnected() && this->mqttClient != nullptr
			&& (!mqttClient->connected())
			&& (strcmp(getMqttServer(), "") != 0)
			&& (strcmp(getMqttPort(), "") != 0)) {
			wlog->notice(F("Connect to MQTT server: %s; user: '%s'; password: '%s'; clientName: '%s'"),
					getMqttServer(), getMqttUser(), getMqttPassword(), getClientName(true).c_str());
			// Attempt to connect
			this->mqttClient->setServer(getMqttServer(), String(getMqttPort()).toInt());
			if (mqttClient->connect(getClientName(true).c_str(),
					getMqttUser(), //(mqttUser != "" ? mqttUser.c_str() : NULL),
					getMqttPassword(),
					((String)getMqttTopic()+"/"+MQTT_TELE+"/LWT").c_str(), 2, true, // willTopic, WillQos, willRetain
					"Offline", true// willMessage, cleanSession
					)) { //(mqttPassword != "" ? mqttPassword.c_str() : NULL))) {
				wlog->notice(F("Connected to MQTT server."));

				// send Online
				mqttClient->publish(((String)getMqttTopic()+"/"+MQTT_TELE+"/LWT").c_str(), "Online", true);

				//Send device structure and status
				mqttClient->subscribe("devices/#");

				WDevice *device = this->firstDevice;
				while (device != nullptr) {
					String topic("devices/");
					topic.concat(device->getId());
					WStringStream* response = getResponseStream();
					WJson json(response);
					json.beginObject();
					json.propertyString("url", "http://", getDeviceIp().toString().c_str(), "/things/", device->getId());
					json.propertyString("ip", getDeviceIp().toString().c_str());
					json.propertyString("topic", ((String)getMqttTopic()+"/"+MQTT_STAT+"/things/"+device->getId()).c_str());
					json.endObject();
					mqttClient->publish(topic.c_str(), response->c_str());
					device = device->next;
				}
				mqttClient->unsubscribe("devices/#");
				//Subscribe to device specific topic
				String subscribeTopic=String(String(getMqttTopic()) + "/" + String(MQTT_CMND) + "/#");
				wlog->notice(F("Subscribing to Topic %s"),subscribeTopic.c_str());
				mqttClient->subscribe(subscribeTopic.c_str());
				notify(false);
				return true;
			} else {
				wlog->notice(F("Connection to MQTT server failed, rc=%d"), mqttClient->state());
				notify(false);
				return false;
			}
		} else {
			return false;
		}
	}
#endif

	void notify(bool sendState) {
		if (statusLed != nullptr) {
			if (isWifiConnected()) {
				//statusLed->setOn(false);
			} else if (isSoftAP()) {
				//statusLed->setOn(true, 0);
			} else {
				//statusLed->setOn(true, 500);
			}
		}
#ifndef MINIMAL
		if (sendState) {
			WDevice *device = this->firstDevice;
			while (device != nullptr) {
				handleDeviceStateChange(device);
				device = device->next;
			}
		}
#endif
		if (onNotify) {
			onNotify();
		}
	}

	void handleHttpRootRequest(AsyncWebServerRequest *request) {
		wlog->notice(F("handleHttpRootRequest"));
		if (isWebServerRunning()) {
			if (restartFlag==nullptr) {			
				AsyncResponseStream* page = httpHeader(request, "Main");
				printHttpCaption(page);
				WDevice *device = firstDevice;
				page->printf_P(HTTP_BUTTON, "wifi", "get", F("Configure Network"));
				while (device != nullptr) {
					if (device->isProvidingConfigPage()) {
						String s("Configure ");
						s.concat(device->getName());
						page->printf_P(HTTP_BUTTON, device->getId(), "get", s.c_str());
						//page->printAndReplace(FPSTR(HTTP_BUTTON_DEVICE), device->getId(), device->getName());
					}
					WPage *subpage = device->firstPage;
					while (subpage != nullptr) {
						String url =(String)device->getId()+"_"+(String)subpage->getId();
						page->printf_P(HTTP_BUTTON, url.c_str() , "get", subpage->getTitle());
						//page->printf_P(HTTP_BUTTON, "YY", "get", "XX");
						//Serial.printf("YYY1: %s\n", url.c_str());
						//Serial.printf("YYY2: %s\n", subpage->getTitle());
						subpage = subpage->next;
					}
					device = device->next;
				}
				page->printf_P(HTTP_BUTTON, "firmware", "get", F("Update firmware"));
				page->printf_P(HTTP_BUTTON, "info", "get", "Info");
				page->printf_P(HTTP_BUTTON, "reset", "post", "Reboot");
				httpFooter(page);
				request->send(page);
			} else {
				AsyncResponseStream* page = httpHeader(request, "Info", F("<meta http-equiv=\"refresh\" content=\"10\">"));
				page->print(restartFlag);
				page->print(F("<br><br>"));
				page->print(F("Module will reset in a few seconds..."));
				httpFooter(page);
				request->send(page);
			}
		}
	}

	void handleHttpDeviceConfiguration(AsyncWebServerRequest *request, WDevice *device) {
		if (isWebServerRunning()) {
			wlog->notice(F("Device config page"));
			AsyncResponseStream* page = httpHeader(request, F("Device Configuration"));
			printHttpCaption(page);
			device->printConfigPage(request, page);
			httpFooter(page);
			request->send(page);
					}

	}
	void handleHttpDevicePage(AsyncWebServerRequest *request, WDevice *device, WPage *subpage) {
		if (isWebServerRunning()) {
			wlog->notice(F("Device subpage"));
			AsyncResponseStream* page = httpHeader(request, subpage->getTitle());
			printHttpCaption(page);
			subpage->printPage(request, page);
			httpFooter(page);
			request->send(page);
					}
	}

	void handleHttpDevicePageSubmitted(AsyncWebServerRequest *request, WDevice *device, WPage *subpage) {
		if (isWebServerRunning()) {
			wlog->notice(F("handleHttpDevicePageSubmitted "), device->getId());
			AsyncResponseStream* page = httpHeader(request, subpage->getTitle());
			printHttpCaption(page);
			subpage->submittedPage(request, page);
			page->printf_P(HTTP_HOME_BUTTON);
			httpFooter(page);
			request->send(page);
						wlog->notice(F("handleHttpDevicePageSubmitted Done"));
		}
	}

	void handleHttpNetworkConfiguration(AsyncWebServerRequest *request) {
		if (isWebServerRunning()) {
			// stop timer 
			// resetWifiTimeout
			if (wifiModeRunning == wnWifiMode_t::WIFIMODE_FALLBACK) this->apStartedAt=millis();
			wlog->notice(F("Network config page"));
			AsyncResponseStream* page = httpHeader(request, F("Network Configuration"));
			printHttpCaption(page);
			page->printf_P(HTTP_CONFIG_PAGE_BEGIN, "");
			page->printf_P(HTTP_PAGE_CONFIGURATION_STYLE, (this->isSupportingMqtt() ? F("block") : F("none")));
			page->printf_P(HTTP_TEXT_FIELD, F("Hostname/Idx:"), "i", "16", getIdx());
			page->printf_P(HTTP_TEXT_FIELD, F("Wifi ssid (only 2.4G):"), "s", "32", getSsid());
			page->printf_P(HTTP_PASSWORD_FIELD, F("Wifi password:"), "p", "p", "p", "32", (strlen(getPassword()) ?  FORM_PW_NOCHANGE : ""));
			page->printf_P(HTTP_PAGE_CONFIGURATION_OPTION, "apfb", (this->isSupportingApFallback() ? HTTP_CHECKED : ""),
			"", HTTP_PAGE_CONFIIGURATION_OPTION_APFALLBACK);
			//mqtt
			page->printf_P(HTTP_PAGE_CONFIGURATION_OPTION, "mq", (this->isSupportingMqtt() ? HTTP_CHECKED : ""), 
				F("id='mqttEnabled'onclick='hideMqttGroup()'"), HTTP_PAGE_CONFIIGURATION_OPTION_MQTT);
			page->printf_P(HTTP_PAGE_CONFIGURATION_MQTT_BEGIN);
			page->printf_P(HTTP_TEXT_FIELD, F("MQTT Server:"), "ms", "32", getMqttServer());
			page->printf_P(HTTP_TEXT_FIELD, F("MQTT Port:"), "mo", "4", getMqttPort());
			page->printf_P(HTTP_TEXT_FIELD, F("MQTT User:"), "mu", "16", getMqttUser());
			page->printf_P(HTTP_PASSWORD_FIELD, F("MQTT Password:"), "mp", "mp", "mp", "32", (strlen(getMqttPassword()) ?  FORM_PW_NOCHANGE : ""));
			page->printf_P(HTTP_TEXT_FIELD, F("Topic, e.g.'home/room':"), "mt", "32", getMqttTopic());

			page->printf_P(HTTP_PAGE_CONFIGURATION_OPTION, F("mqhass"), (this->isSupportingMqttHASS() ? HTTP_CHECKED : ""),
				"", HTTP_PAGE_CONFIIGURATION_OPTION_MQTTHASS);
			page->printf_P(HTTP_PAGE_CONFIGURATION_OPTION, F("mqsv"), (this->isSupportingMqttSingleValues() ? HTTP_CHECKED : ""),
				"", HTTP_PAGE_CONFIIGURATION_OPTION_MQTTSINGLEVALUES);

			page->printf_P(HTTP_PAGE_CONFIGURATION_MQTT_END);
			page->printf_P(HTTP_CONFIG_SAVE_BUTTON);
			httpFooter(page);
			request->send(page);
					}
	}

	void handleHttpsave(AsyncWebServerRequest *request) {
		if (isWebServerRunning()) {
#ifndef MINIMAL
			// remove old autoconfiguration
			sendMqttHassAutodiscover(true);
#endif			
			this->idx->setString(getValueOrEmpty(request, "i").c_str());
			this->ssid->setString(getValueOrEmpty(request, "s").c_str());
			settings->setString(PROP_PASSWORD,  (getValueOrEmpty(request, "p").equals(FORM_PW_NOCHANGE) ? getPassword() : getValueOrEmpty(request, "p").c_str())) ;
			settings->setString(PROP_MQTTSERVER, getValueOrEmpty(request, "ms").c_str());
			String mqtt_port = getValueOrEmpty(request, "mo");
			settings->setString(PROP_MQTTPORT, (mqtt_port != "" ? mqtt_port.c_str() : "1883"));
			settings->setString(PROP_MQTTUSER, getValueOrEmpty(request, "mu").c_str());
			settings->setString(PROP_MQTTPASSWORD, getValueOrEmpty(request, "mp").equals(FORM_PW_NOCHANGE) ? getMqttPassword() : getValueOrEmpty(request, "mp").c_str());
			this->mqttBaseTopic->setString(getValueOrEmpty(request, "mt").c_str());
			byte nb1 = 0;
			if (getValueOrEmpty(request, "mq").equals("true")) nb1 |= NETBITS1_MQTT;
			if (getValueOrEmpty(request, "mqhass").equals("true")) nb1 |= NETBITS1_HASS;
			if (getValueOrEmpty(request, "apfb").equals("true")) nb1 |= NETBITS1_APFALLBACK;
			if (getValueOrEmpty(request, "mqsv").equals("true")) nb1 |= NETBITS1_MQTTSINGLEVALUES;
			settings->setByte(PROP_NETBITS1, nb1);
			wlog->notice(F("supportingMqtt set to: %d"), nb1);
			settings->save(); 
			this->restart(request, F("Settings saved."));
			// we must not here this->supportingMqtt -> if it _was_ disabled and we're going to enable it here
			// mqtt would not be initialized but would be used immediately
		}
	}

	void handleHttpSaveDeviceConfiguration(AsyncWebServerRequest *request, WDevice *device) {
		if (isWebServerRunning()) {
			wlog->notice(F("handleHttpSaveDeviceConfiguration "), device->getId());
			device->saveConfigPage(request);
			settings->save();
			if (device->isConfigNeedsReboot()){
				wlog->notice(F("Reboot"));
				this->restart(request, F("Device settings saved."));
			} else {
				this->webReturnStatusPage(request, F("Device settings saved."), "");
			}
			wlog->notice(F("handleHttpSaveDeviceConfiguration Done"));
		}
	}

	void handleHttpInfo(AsyncWebServerRequest *request) {
		if (isWebServerRunning()) {
			AsyncResponseStream* page = httpHeader(request, "Info");
			printHttpCaption(page);
			page->print(F("<table>"));
			// header
			page->print(F("<tr><th colspan=\"2\"><h4>Main</h4></th></tr>"));


			htmlTableRowTitle(page, F("Chip ID:"));
			page->print(ESP.getChipId());
			htmlTableRowEnd(page);

			htmlTableRowTitle(page, F("Flash Chip ID:"));
			page->print(ESP.getFlashChipId());
			htmlTableRowEnd(page);

			htmlTableRowTitle(page, F("IDE Flash Size:"));
			page->print(ESP.getFlashChipSize());
			htmlTableRowEnd(page);

			htmlTableRowTitle(page, F("Real Flash Size:"));
			page->print(ESP.getFlashChipRealSize());
			htmlTableRowEnd(page);

			htmlTableRowTitle(page, F("IP address:"));
			page->print(this->getDeviceIp().toString());
			htmlTableRowEnd(page);

			htmlTableRowTitle(page, F("MAC address:"));
			page->print(WiFi.macAddress());
			htmlTableRowEnd(page);

			htmlTableRowTitle(page, F("Current WiFi RSSI:"));
			page->print(WiFi.RSSI());
			page->print(" dBm");
			htmlTableRowEnd(page);

			htmlTableRowTitle(page, F("Flash Chip size:"));
			page->print(ESP.getFlashChipSize());
			page->print(STR_BYTE);
			htmlTableRowEnd(page);

			htmlTableRowTitle(page, F("Current sketch size:"));
			page->print(ESP.getSketchSize());
			page->print(STR_BYTE);
			htmlTableRowEnd(page);

			htmlTableRowTitle(page, F("Available sketch size:"));
			page->print(ESP.getFreeSketchSpace());
			page->print(STR_BYTE);
			htmlTableRowEnd(page);

			htmlTableRowTitle(page, F("Free heap size:"));
			page->print(ESP.getFreeHeap());
			page->print(STR_BYTE);
			htmlTableRowEnd(page);

			htmlTableRowTitle(page, F("Largest free heap block:"));
			page->print(ESP.getMaxFreeBlockSize());
			page->print(STR_BYTE);
			htmlTableRowEnd(page);

			htmlTableRowTitle(page, F("Heap fragmentation:"));
			page->print(ESP.getHeapFragmentation());
			page->print(" %");
			htmlTableRowEnd(page);
			
			htmlTableRowTitle(page, F("Uptime:"));
			unsigned long secs=millis()/1000;
			unsigned int days = secs / (60 * 60 * 24);
			secs -= days * (60 * 60 * 24);
			unsigned int hours = secs / (60 * 60);
			secs -= hours * (60 * 60);
			unsigned int minutes = secs / 60;
			secs -= minutes * 60;
			page->printf_P("%dd, %dh, %dm, %ds",
			days, hours, minutes, secs);
			htmlTableRowEnd(page);

			WDevice *device = this->firstDevice;
			while (device != nullptr) {
				if (device->hasInfoPage()){
					page->print(F("<tr><th colspan=\"2\"><h4>"));
					page->print(device->getName());
					page->print(F("</h4></th></tr>"));
					device->printInfoPage(page);
				}
				device = device->next;
			}
			page->print(F("</table>"));
			page->print(F("<br/><br/>"));
			page->print(FPSTR(HTTP_HOME_BUTTON));
			httpFooter(page);
			request->send(page);
		}
	}

	/** Handle the reset page */
	void handleHttpReset(AsyncWebServerRequest *request) {
		if (isWebServerRunning()) {
			this->restart(request, F("Resetting was caused manually by web interface. "));
		}
	}

	void printHttpCaption(AsyncResponseStream* page) {
		page->print(F("<h2>"));
		page->print(applicationName);
		page->print(" ");
		page->print(firmwareVersion);
		page->print(debug ? " (debug)" : "");
		page->print(F("</h2>"));
		if (getIdx()){
			page->print(F("<h3>"));
			page->print(getIdx());
			page->print(F("</h3>"));
		}

	}

	template<class T, typename ... Args> AsyncResponseStream * httpHeader(AsyncWebServerRequest *request, T title, const __FlashStringHelper * HeaderAdditional) {
		if (!isWebServerRunning()) return nullptr;
		AsyncResponseStream *page = request->beginResponseStream(TEXT_HTML, 3096U);
		page->printf_P(HTTP_HEAD_BEGIN, getIdx(), title);
		page->print(FPSTR(HTTP_HEAD_SCRIPT));
		page->print(FPSTR(HTTP_HEAD_STYLE));
		if (HeaderAdditional!=nullptr) page->print(FPSTR(HeaderAdditional));
		page->print(FPSTR(HTTP_HEAD_END));
		return page;
	}

	template<class T, typename ... Args> AsyncResponseStream * httpHeader(AsyncWebServerRequest *request, T title) {
		return httpHeader(request, title, NULL);
	}

	void httpFooter(AsyncResponseStream  *page) {
		if (!isWebServerRunning()) return;
		page->print(FPSTR(HTTP_BODY_END));
	}


	String getClientName(bool lowerCase) {
		String result = (applicationName.equals("") ? "ESP" : String(applicationName));
		result.replace(" ", "-");
		if (lowerCase) {
			result.replace("-", "");
			result.toLowerCase();
		}
		//result += "_";
		String chipId = String(ESP.getChipId());
		int resLength = result.length() + chipId.length() + 1 - 32;
		if (resLength > 0) {
			result.substring(0, 32 - resLength);
		}
		return result + "-" + chipId;
	}

	String getHostName() {
		String hostName = getIdx();
		hostName.replace(".", "-");
		hostName.replace(" ", "-");
		if (hostName.equals("")) {
			hostName = getClientName(false);
		}
		return hostName;
	}

	void handleHttpFirmwareUpdate(AsyncWebServerRequest *request) {
		if (isWebServerRunning()) {
			AsyncResponseStream* page = httpHeader(request, F("Firmware update"));
			printHttpCaption(page);
			page->printf_P(HTTP_FORM_FIRMWARE);
			page->print(F("Available sketch size: "));
			page->print(ESP.getFreeSketchSpace());
			page->print(STR_BYTE);
			page->printf_P(HTTP_BODY_END);
			httpFooter(page);
			request->send(page);
					}
	}

	void handleHttpFirmwareUpdateFinished(AsyncWebServerRequest *request) {
		if (isWebServerRunning()) {
			if (Update.hasError()) {
				this->restart(request, firmwareUpdateError);
			} else {
				this->restart(request, F("Update successful."));
			}
		}
	}

	void handleHttpFirmwareUpdateProgress(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
		//Start firmwareUpdate
		this->updateRunning = true;
#ifndef MINIMAL
		//Close existing MQTT connections
		this->disconnectMqtt();
#endif

		if (!index){
			firmwareUpdateError = nullptr;
			unsigned long free_space = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
			wlog->notice(F("Update starting: %s"), filename.c_str());
			Update.runAsync(true);
			if (!Update.begin(free_space)) {
				setFirmwareUpdateError("Can't start update (" + String(free_space) + "): ");
			}
		}
		if(!Update.hasError()){
			if(Update.write(data, len) != len){
				setFirmwareUpdateError("Can't upload file: ");
			}
		}
		if (final){
			if (Update.end(true)) { //true to set the size to the current progress
				wlog->notice(F("Update complete: "));
			} else {
				setFirmwareUpdateError("Can't finish update: ");
			}
		}
	}

	const __FlashStringHelper* getFirmwareUpdateErrorMessage() {
		switch (Update.getError()) {
		case UPDATE_ERROR_OK:
			return F("No Error");
		case UPDATE_ERROR_WRITE:
			return F("Flash Write Failed");
		case UPDATE_ERROR_ERASE:
			return F("Flash Erase Failed");
		case UPDATE_ERROR_READ:
			return F("Flash Read Failed");
		case UPDATE_ERROR_SPACE:
			return F("Not Enough Space");
		case UPDATE_ERROR_SIZE:
			return F("Bad Size Given");
		case UPDATE_ERROR_STREAM:
			return F("Stream Read Timeout");
		case UPDATE_ERROR_MD5:
			return F("MD5 Failed: ");
		case UPDATE_ERROR_SIGN:
			return F("Signature verification failed");
		case UPDATE_ERROR_FLASH_CONFIG:
			return F("Flash config wrong.");
		case UPDATE_ERROR_NEW_FLASH_CONFIG:
			return F("New Flash config wrong.");
		case UPDATE_ERROR_MAGIC_BYTE:
			return F("Magic byte is wrong, not 0xE9");
		case UPDATE_ERROR_BOOTSTRAP:
			return F("Invalid bootstrapping state, reset ESP8266 before updating");
		default:
			return F("UNKNOWN");
		}
	}

	void setFirmwareUpdateError(String msg) {
		firmwareUpdateError = getFirmwareUpdateErrorMessage();
		String s = msg + firmwareUpdateError;
		wlog->notice(s.c_str());
	}

	void webReturnStatusPage(AsyncWebServerRequest *request, const __FlashStringHelper* reasonMessage1, const char* reasonMessage2) {
		AsyncResponseStream* page = httpHeader(request, reasonMessage1);
		printHttpCaption(page);
		page->printf_P(HTTP_SAVED, reasonMessage1, reasonMessage2);
		page->printf_P(HTTP_HOME_BUTTON);
		httpFooter(page);
		request->send(page);
	}
	
	void restart(AsyncWebServerRequest *request, const __FlashStringHelper* reasonMessage) {
		this->restartFlag = reasonMessage;
		webReturnStatusPage(request, reasonMessage, "ESP reboots now...");
	}

	bool loadSettings(){
		wlog->trace(F("loading settings, getNetworkSettingsVersion: %d"), settings->getNetworkSettingsVersion());
		if (settingsOld){
			if (settingsOld->getNetworkSettingsVersion()==NETWORKSETTINGS_PRE_FAS114){
				wlog->notice(F("Reading NetworkSettings PRE_FAS114"));
				settingsOld->setString(PROP_IDX, 32, "");
				settingsOld->setString(PROP_SSID, 32, "");
				settingsOld->setString(PROP_PASSWORD, 64, "");
				settingsOld->setByte(PROP_NETBITS1, (NETBITS1_MQTT | NETBITS1_HASS));
				settingsOld->setString(PROP_MQTTSERVER, 32, "");
				settingsOld->setString(PROP_MQTTPORT, 4, "1883");
				settingsOld->setString(PROP_MQTTUSER, 32, "");
				settingsOld->setString(PROP_MQTTPASSWORD, 64, "");
				settingsOld->setString(PROP_MQTTTOPIC, 64, "");
			} else if (settingsOld->getNetworkSettingsVersion()==NETWORKSETTINGS_PRE_102 || settingsOld->getNetworkSettingsVersion()==NETWORKSETTINGS_PRE_109){
				wlog->notice(F("Reading NetworkSettings PRE_109/PRE_102"));
				settingsOld->setString(PROP_IDX, 32, "");
				settingsOld->setString(PROP_SSID, 64, "");
				settingsOld->setString(PROP_PASSWORD, 32, "");
				settingsOld->setByte(PROP_NETBITS1, (NETBITS1_MQTT | NETBITS1_HASS));
				settingsOld->setString(PROP_MQTTSERVER, 32, "");
				settingsOld->setString(PROP_MQTTPORT, 4, "1883");
				settingsOld->setString(PROP_MQTTUSER, 32, "");
				settingsOld->setString(PROP_MQTTPASSWORD, 64, "");
				settingsOld->setString(PROP_MQTTTOPIC, 64, "");
				if (settingsOld->getNetworkSettingsVersion()==NETWORKSETTINGS_PRE_109){
					wlog->notice(F("Reading NetworkSettings PRE_109"));
					settingsOld->setString("mqttStateTopic", 16, DEFAULT_TOPIC_STATE); // unused
					settingsOld->setString("mqttSetTopic", 16, DEFAULT_TOPIC_SET); // unused
				}
			}
			settingsOld->addingNetworkSettings = false;
		}

		
		this->idx = settings->setString(PROP_IDX, 16,
			(settingsOld && settingsOld->existsSetting(PROP_IDX) ? settingsOld->getString(PROP_IDX) : this->getClientName(true).c_str()));
		this->ssid = settings->setString(PROP_SSID, 32, (settingsOld && settingsOld->existsSetting(PROP_SSID) ? settingsOld->getString(PROP_SSID) : ""));
		settings->setString(PROP_PASSWORD, 32, (settingsOld && settingsOld->existsSetting(PROP_PASSWORD) ? settingsOld->getString(PROP_PASSWORD) : ""));
		this->netBits1 = settings->setByte(PROP_NETBITS1, (settingsOld && settingsOld->getByte(PROP_NETBITS1) ? settingsOld->getByte(PROP_NETBITS1) : (NETBITS1_MQTT | NETBITS1_HASS)));
		settings->setString(PROP_MQTTSERVER, 32, (settingsOld && settingsOld->existsSetting(PROP_MQTTSERVER) ? settingsOld->getString(PROP_MQTTSERVER) : ""));
		settings->setString(PROP_MQTTPORT, 4, (settingsOld && settingsOld->existsSetting(PROP_MQTTPORT) ? settingsOld->getString(PROP_MQTTPORT) : "1883"));
		settings->setString(PROP_MQTTUSER, 16, (settingsOld && settingsOld->existsSetting(PROP_MQTTUSER) ? settingsOld->getString(PROP_MQTTUSER) : ""));
		settings->setString(PROP_MQTTPASSWORD, 32, (settingsOld && settingsOld->existsSetting(PROP_MQTTPASSWORD) ? settingsOld->getString(PROP_MQTTPASSWORD) : ""));
		this->mqttBaseTopic = settings->setString(PROP_MQTTTOPIC, 32, (settingsOld && settingsOld->existsSetting(PROP_MQTTTOPIC) ? settingsOld->getString(PROP_MQTTTOPIC) : getIdx()));
		this->mqttStateTopic = settings->setString("mqttStateTopic", 16, (settingsOld && settingsOld->existsSetting("mqttStateTopic") ? settingsOld->getString("mqttStateTopic") : DEFAULT_TOPIC_STATE)); // unused
		this->mqttSetTopic = settings->setString("mqttSetTopic", 16, (settingsOld && settingsOld->existsSetting("mqttSetTopic") ? settingsOld->getString("mqttSetTopic") : DEFAULT_TOPIC_SET)); // unused

		// Split mqtt setting into bits - so we keep settings storage compatibility
		if (this->netBits1->getByte() == 0xFF) this->netBits1->setByte(NETBITS1_MQTT | NETBITS1_HASS); // compatibility
		this->supportingMqtt = new WProperty("supportingMqtt", "supportingMqtt", BOOLEAN);
		this->supportingMqtt->setBoolean(this->netBits1->getByte() & NETBITS1_MQTT);
		this->supportingMqtt->setReadOnly(true);
		this->supportingMqttHASS = new WProperty("supportingMqttHASS", "supportingMqttHASS", BOOLEAN);
		this->supportingMqttHASS->setBoolean(this->netBits1->getByte() & NETBITS1_HASS);
		this->supportingMqttHASS->setReadOnly(true);

		this->supportingApFallback= new WProperty("supportingApFallback", "supportingApFallback", BOOLEAN);
		this->supportingApFallback->setBoolean(this->netBits1->getByte() & NETBITS1_APFALLBACK);
		this->supportingApFallback->setReadOnly(true);

		this->supportingMqttSingleValues= new WProperty("supportingMqttSingleValues", "supportingMqttSingleValues", BOOLEAN);
		this->supportingMqttSingleValues->setBoolean(this->netBits1->getByte() & NETBITS1_MQTTSINGLEVALUES);
		this->supportingMqttSingleValues->setReadOnly(true);

		settings->addingNetworkSettings = false;
		bool settingsStored = (settings->existsSettingsNetwork() || (settingsOld && settingsOld->existsSettingsNetwork()));
		if (settingsStored) {

			wlog->notice(F("SettingsStored!"));

			if (getMqttTopic() == "") {
				this->mqttBaseTopic->setString(this->getClientName(true).c_str());
			}
#ifndef MINIMAL
			if ((isSupportingMqtt()) && (this->mqttClient != nullptr)) {
				this->disconnectMqtt();
			}
#endif
			settingsStored = ((strcmp(getSsid(), "") != 0)
					&& (((isSupportingMqtt()) && (strcmp(getMqttServer(), "") != 0) && (strcmp(getMqttPort(), "") != 0)) || (isSupportingWebThing())));
			if (settingsStored) {
				wlog->notice(F("Settings loaded successfully:"));
			} else {
				wlog->notice(F("Settings are not complete:"));
			}
			wlog->notice(F("SSID: '%s'; MQTT enabled: %d; MQTT server: '%s'; MQTT port: %s; WebThings enabled: %d"),
								getSsid(), isSupportingMqtt(), getMqttServer(), getMqttPort(), isSupportingWebThing());

		}
		EEPROM.end();
		return settingsStored;
	}

	void handleUnknown(AsyncWebServerRequest *request) {
		wlog->warning(F("Webserver: 404: '%s'"), request->url().c_str());
		request->send(404, "text/plain", "404: Not found");
	}

	void sendDevicesStructure(AsyncWebServerRequest* request) {
		wlog->notice(F("Send description for all devices... "));
		WStringStream* response = getResponseStream();
		WJson json(response);
		json.beginArray();
		WDevice *device = this->firstDevice;
		while (device != nullptr) {
			if (device->isVisible(WEBTHING)) {
				device->toJsonStructure(&json, "", WEBTHING);
			}
			device = device->next;
		}
		json.endArray();
		request->send(200, APPLICATION_JSON, response->c_str());
		wlog->notice(F("DONE"));
		delete response;
	}

	void sendDeviceStructure(AsyncWebServerRequest *request, WDevice *device) {
		wlog->notice(F("Send description for device: %s"), device->getId());
		WStringStream* response = getResponseStream();
		WJson json(response);
		device->toJsonStructure(&json, "", WEBTHING);
		request->send(200, APPLICATION_JSON, response->c_str());
		delete response;
	}

	void sendDeviceValues(AsyncWebServerRequest *request, WDevice *device) {
		wlog->notice(F("Send all properties for device: "), device->getId());
		WStringStream* response = getResponseStream();
		WJson json(response);
		json.beginObject();
		if (device->isMainDevice()) {
			json.propertyString(PROP_IDX, getIdx());
			json.propertyString("ip", getDeviceIp().toString().c_str());
			json.propertyString("firmware", firmwareVersion.c_str());
		}
		device->toJsonValues(&json, WEBTHING);
		json.endObject();
		request->send(200, APPLICATION_JSON, response->c_str());
		delete response;
	}

#ifndef MINIMAL
	void handleDevicesChangedPropertiesMQTT() {
		if (!isMqttConnected()) return;
		if (!isSupportingMqttSingleValues()) return;
		WDevice *device = this->firstDevice;
		while (device != nullptr) {
			if (device->isVisible(MQTT) and device->isMqttSendChangedValues()) {
				WProperty* property = device->firstProperty;
				while (property != nullptr) {
					if (property->isVisible(MQTT) && property->isChanged() && !property->isNull()) {
						String stat_topic = String(getMqttTopic()) + String("/") + String(MQTT_STAT) + String("/things/") + String(device->getId()) + String("/properties/") + String(property->getId());
						wlog->trace(F("sending changed property '%s' with value '%s' for device '%s' to topic '%s'"),
							property->getId(), property->toString().c_str(), device->getId(), stat_topic.c_str());
						publishMqtt(stat_topic.c_str(), property->toString().c_str(), device->isMqttRetain());
						property->setUnChanged();

						// only one per loop() -> bye
						return;
					}
					property = property->next;
				}
			}
			device = device->next;
		}
	}
#endif

	void getPropertyValue(AsyncWebServerRequest *request, WProperty *property) {
		WStringStream* response = getResponseStream();
		WJson json(response);
		json.beginObject();
		property->toJsonValue(&json);
		json.endObject();
		property->setRequested(true);
		wlog->trace(F("getPropertyValue %s"), response->c_str());
		//FIXMEwebServer->send(200, APPLICATION_JSON, response->c_str());

	}

	void setPropertyValue(AsyncWebServerRequest *request, WDevice *device) {
		if (request->hasArg("plain") == false) {
			request->send(422); // Unprocessable Entity
			return;
		}
		WJsonParser parser;
		WProperty* property = parser.parse(request->getParam("plain")->value().c_str(), device);
		if (property != nullptr) {
			//response new value
			wlog->notice(F("Set property value: %s (web request) %s"), property->getId(), request->getParam("plain")->value().c_str());
			WStringStream* response = getResponseStream();
			WJson json(response);
			json.beginObject();
			property->toJsonValue(&json);
			json.endObject();
			request->send(200, APPLICATION_JSON, response->c_str());
		} else {
			// unable to parse json
			wlog->notice(F("unable to parse json: %s"), request->getParam("plain")->value().c_str());
			request->send(500);
		}
	}

	void sendErrorMsg(int status, const char *msg) {
		//FIXME
		#if 0
		WStringStream* response = getResponseStream();
		WJson json(response);
		json.beginObject();
		json.propertyString("error", msg);
		json.propertyInteger("status", status);
		json.endObject();
		webServer->send(200, APPLICATION_JSON, response->c_str());
		#endif
	}

	void bindWebServerCallsNetwork(WDevice *device) {
		wlog->notice(F("Bind webServer calls for device %s"), device->getId());
		String deviceBase("/things/");
		deviceBase.concat(device->getId());
		#if 0
		WProperty *property = device->firstProperty;
		while (property != nullptr) {
			if (property->isVisible(WEBTHING)) {
				String propertyBase = deviceBase + "/properties/" + property->getId();
				webServer->on(propertyBase.c_str(), HTTP_GET, [this, property](AsyncWebServerRequest *request){
					getPropertyValue(request, property);
				});
				webServer->on(propertyBase.c_str(), HTTP_PUT, [this, device](AsyncWebServerRequest *request){
					setPropertyValue(request, device);
				});
			}
			property = property->next;
		}
		#endif
		String propertiesBase = deviceBase + "/properties";
		webServer->on(propertiesBase.c_str(), HTTP_GET,	[this, device](AsyncWebServerRequest *request){
			sendDeviceValues(request, device);
		});
		webServer->on(deviceBase.c_str(), HTTP_GET,	[this, device](AsyncWebServerRequest *request){
			sendDeviceStructure(request, device);
		});
		device->bindWebServerCalls(webServer);
	}

	WDevice* getDeviceById(const char* deviceId) {
		WDevice *device = this->firstDevice;
		while (device != nullptr) {
			if (strcmp(device->getId(), deviceId) == 0) {
				return device;
			}
			device = device->next;
		}
		return nullptr;
	}

	void handleWebSocket() {
	}

};

#endif