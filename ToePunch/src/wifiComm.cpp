#include "wifiComm.h"
#include <esp_wifi.h>
#include <esp_bt.h>
#include "driver/adc.h"

#include "info.h"
#include "md5.h"
#include "str.h"

#include "lwip/apps/sntp.h"
//#include "ESPRandom.h"

#define DEBUG 

#ifdef DEBUG
  #define DEBUG_PRINT(...) { Serial.print(__VA_ARGS__); }
  #define DEBUG_PRINTF(...) { Serial.printf(__VA_ARGS__); }
  #define DEBUG_PRINTLN(...) { Serial.println(__VA_ARGS__); }
#else
  #define DEBUG_PRINT(...) {}
  #define DEBUG_PRINTF(...) {}
  #define DEBUG_PRINTLN(...) {}
#endif  

WifiComm comm; 

AsyncClient* aClient = NULL;
    
char* WifiComm::ssid;
char* WifiComm::password;
IPAddress WifiComm::localIP;
IPAddress WifiComm::gateway;
IPAddress WifiComm::subnet;

uint16_t  WifiComm::hostPort;
IPAddress WifiComm::hostIP;
char* WifiComm::hostName;
char* WifiComm::hostURL;
char* WifiComm::statusName;

char WifiComm::request[16384];	// pole pro sestavovani dotazu
uint32_t WifiComm::pos; // pozice v poli znaku pri sestavovani dotazu

WifiComm::TState WifiComm::state = WifiComm::IDLE;
uint8_t  WifiComm::allRecords = 0;
uint16_t WifiComm::addrRecordConfirm;
uint16_t WifiComm::addrRecordStart;
uint16_t WifiComm::addrRecordEnd;
uint32_t WifiComm::count = 0;
uint32_t WifiComm::countSum = 0;
uint8_t  WifiComm::error = 0;

bool WifiComm::statusUpload = 0;
float WifiComm::voltage;

uint32_t timeComm = 0;

struct InfoNTP {
	uint32_t started;
	uint32_t finished;
	uint32_t applied;
	uint32_t reset;
};
InfoNTP infoNTP;

WifiComm::WifiComm() {

}

void WifiComm::setStatus(uint32_t time) {
	statusName = Store::status.hostName;
	statusActive = 0;
	statusActivePeriod = Store::status.act != 0 ? Store::status.act * 60 : NEXT_STATUS_NEVER;
	statusPassivePeriod = Store::status.pas != 0 ? Store::status.pas * 60 : NEXT_STATUS_NEVER;
	statusStamp = time;

	DEBUG_PRINTF("Active: %d Passive: %d\r\n", statusActivePeriod, statusPassivePeriod);
}

void WifiComm::init() {
	url_encoder_rfc_tables_init();

  ssid = Store::wifi.ssid;
  password = Store::wifi.pass;
  
	hostIP = Store::server.IP;
	hostName = Store::server.hostName;
	hostURL = Store::server.hostURL;
  hostPort = Store::server.port;

	uint32_t time = rtc.getTime();

	setStatus(time);
  state = IDLE;

	Serial.printf("Event time: %d Current time: %d\n\r", Store::event.time, time);
	if (time < Store::event.time) { // Kdyz je cas mensi nez cas 00, tak nastav cas vysilani na zacatek zavodu
		lastTime = Store::event.time + Store::station.id * 5; // + SI ... posun casu, aby nevysilaly soucasne
		Serial.printf("Event time -> lastTime: %d\n\r", lastTime);
	} else { // Kdyz je cas vetsi nez cas 00, tak nastav cas vysilani od ted
		lastTime = time;
		Serial.printf("Now -> lastTime: %d\n\r", lastTime);
	}
	setNextStatus(time);
}

bool WifiComm::setNextStatus(uint32_t time) {	
	if (statusActive) {
		if (time - statusStamp > statusActivePeriod) {
			statusStamp = time;
			statusActive = time - Store::lastPunchTime < statusActivePeriod;
			return 1;
		}
	} else {
		if (time - Store::lastPunchTime < statusActivePeriod &&
				statusStamp + statusPassivePeriod > Store::lastPunchTime + statusActivePeriod) {
			statusActive = 1;
			statusStamp = Store::lastPunchTime;
		} else {
			if (time - statusStamp > statusPassivePeriod) {
				statusStamp = time;
				return 1;
			}
		}
	}
	return 0;
}

void WifiComm::start(uint8_t all) {
	// all = 0 ... zahajeni uploadu automaticky v case (rezim OnTime)
	// all = 1 ... zahajeni uploadu po razeni (pro rezim OnPunch, jinak se ignoruje)
	// all = 2 ... zahajeni uploadu na vyzadani - Incremental (vsechny rezimy)
	// all = 3 ... zahajeni uploadu na vyzadani - Event (vsechny rezimy)
	// all = 4 ... zahajeni uploadu na vyzadani - Full (vsechny rezimy)
	// all = 255 . zahajeni odesilani stavu
	// all = 254 . zahajeni odesilani procesu nastaveni casu - ladeni FORST

	if (ssid[0] == 0 || ssid[0] == 0xFF) { // Neni nastaveno WiFi
		return;
	}

	error = 0;
  statusUpload = (all > 250);

  if (!statusUpload) {
		if (Store::server.upload != ONPUNCH && all == 1) return; 

		allRecords = (all > 2) ? all : 0; // priznak vynuceni odeslani vsech dat (Event, Full)
		DEBUG_PRINTF("AllRecords: %d IsData: %d ", allRecords, Store::isData());
		if (allRecords == 4) {
			addrRecordStart = EEPROM_TOP;
			addrRecordEnd = 0;
		} else { // Incremental, Event
			if (!allRecords && !Store::isData()) {
				Serial.println("Nothing to upload");
				info.showAsync("Nothing", "to upload", ASYNC_TIMEOUT);
				return; // neni vynuceno odeslani a nejsou data k odeslani
			}
			if (allRecords == 3) {
				if (Store::addrBase == Store::addrActual) {
					Serial.println("Nothing to upload");
					info.showAsync("Nothing", "to upload", ASYNC_TIMEOUT);
					return; // nejsou data k odeslani (zavod je prazdny)
				} else {
					addrRecordStart = Store::addrBase;
				}
			} else {
				addrRecordStart = Store::addrToSend;
			}
			addrRecordEnd = Store::addrActual;
		}
		countSum = 0;
		DEBUG_PRINTF("Start: %d End: %d\r\n", addrRecordStart, addrRecordEnd);
	} else { // Status upload
		allRecords = all != 255;
		voltage = getBattery();
	}

  if (state == IDLE) {
		Serial.println("===================");
		Serial.println("Wifi start");
		state = WIFI_START;
		if (aClient == NULL) {
			aClient = new AsyncClient;
			aClient->onData(&onData, aClient);
			aClient->onConnect(&onConnect, aClient);
			aClient->onDisconnect(&onDisconnect, aClient);
			aClient->onTimeout(&onTimeout, aClient); // pozor spadne
			aClient->onError(&onError, aClient);
			aClient->setAckTimeout(200);	// ms
			aClient->setRxTimeout(3);			// s
		}
	}
}

static char antHostName[16];

bool WifiComm::process() {
	static uint8_t loopCount = 0;
  if (state == IDLE) {
		if (loopCount > 0) { // Testovat cas odesilani jen jednou za 10s, cteni casu ~300us
			loopCount--;
		} else {
			loopCount  = 100;

		  if (Store::server.upload >= ONTIME) {
			// pokud je volno, zkontroluj, zda je cas k odeslani
				int32_t interval = Store::server.upload * 60; // 2 - 255 ... minut
				uint32_t time = rtc.getTime();
				if ((int32_t)(time - lastTime) > interval) {
					lastTime = time;
					start();
				}
			}

		  if (Store::status.act != 0 || Store::status.pas != 0) { // Povoleno odesilani stavu
				uint32_t time = rtc.getTime();
				if (setNextStatus(time)) { // Otestuje, zda odesilat stav a nastavi dalsi 
				  char s[] = "DD.MM.YYYY hh:mm:ss";
  				DEBUG_PRINTF("Status upload: %s\n\r", rtc.printTime(s, time));
					start(255);
				}
			}
		}
  }

  if (state == WIFI_START) {
    connect();
		info.showAsync("Wifi", "connect ..");
  }

  if (state == WIFI_CONNECTING) {
  	if (WiFi.status() == WL_CONNECTED) {
		  Serial.println("connected");
			info.showAsync("Wifi", "connected");
		  state = WIFI_CONNECTED;
    } else if (millis() - timeOut > WIFI_TIMEOUT) { // Timeout 10s pro pripojeni k wifi
			Serial.println("timeout connection");
			info.showAsync("Timeout", "connection", ASYNC_TIMEOUT);
			wifiPowerDown();
		}
	}

	if (state == WIFI_CONNECTED) {
/*
		Serial.print("Host IP: ");
		Serial.println(hostIP);
		Serial.print("Host Port: ");
		Serial.println(hostPort);
*/
    if (!statusUpload) {
			// pro pripojeni na mobil 
			if (Store::server.port == 1968) { // ANT master server
				hostIP = WiFi.gatewayIP();
				sprintf(antHostName , "%d.%d.%d.%d", hostIP[0], hostIP[1], hostIP[2], hostIP[3]);
				hostName = antHostName;
			}
	//		Serial.printf("SSID: %s, Pass: %s IP: %s Host: %s\n\r", ssid, password, hostIP, hostName);
			aClient->connect(hostIP, hostPort);
			state = SERVER_CONNECTING;
			Serial.printf("Connect to server ... %s\r\n", hostName);
  		char s[16];
			info.showAsync("Server", hostName, info.ipToString(s, hostIP));
		} else { // Status upload
			if (Store::status.port == 1968) { // ANT master server
				Store::status.ip = WiFi.gatewayIP();
				sprintf(antHostName , "%d.%d.%d.%d", Store::status.IP[0], Store::status.IP[1], Store::status.IP[2], Store::status.IP[3]);
				statusName = antHostName;
			}
			aClient->connect(Store::status.IP, Store::status.port);
			state = SERVER_CONNECTING;
			Serial.printf("Connect to server ... %s\r\n", statusName);
			Serial.printf("IP: %d.%d.%d.%d\r\n", Store::status.IP[0], Store::status.IP[1], Store::status.IP[2], Store::status.IP[3]);
			char s[16];
			info.showAsync("Server", statusName, info.ipToString(s, Store::status.IP));
		}
	}

	if (state == SERVER_CONNECTED) {
		if (!statusUpload) {
			if ((addrRecordStart != addrRecordEnd) && !error) { // Je neco k odeslani a nenastala chyba
				sendToServer(aClient);			
			} else {
				// Zobrazit OK/Error na displeji
				if (error == 0) Serial.printf("\r\n%d records uploaded ok\r\n", countSum);
				Serial.printf("Exit code: %d\r\n", error);
				aClient->close();
			}	
		} else {
			sendStatus(aClient);
			error = 4;
		}
	}

	if (state == DATA_RECEIVED) {
    if (!statusUpload) {
			state = SERVER_CONNECTED;
		} else { // Status upload
			if (error != 0) {
				info.showAsync("Status", "error", ASYNC_TIMEOUT);
			} 
			Serial.printf("\r\nStatus uploaded - exit code: %d\r\n", error);			
			aClient->close();
		}
	}

	return 1;
} 

bool WifiComm::connect() {
	// connects to access point
	if (ssid[0] == 0 || ssid[0] == 0xFF) {
		info.show("WiFi", "no SSID");
		return 0;
	}
	timeOut = millis();
	timeComm = timeOut;
  Serial.print("Connect to wifi ... ");
	WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  state = WIFI_CONNECTING;
	return 1;
}

char* WifiComm::requestRaw(char* req) {
	char fileName[100];
  char time[] = "YYYYMMDDhhmmss";
  DateTime(rtc.getTime()  + SECONDS_FROM_1970_TO_2000 + SECONDS_FROM_2000_TO_2020).toString(time);

	sprintf(fileName, 
		"%s.%d.%d.txt", 
		time, Store::event.id & 0xFFFFFF, Store::station.id);

	sprintf(req,
	  "POST /%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: ESP32\r\nContent-Type: multipart/form-data; boundary=\"%s\"\r\nContent-Length: %05d\r\n\r\n",
		hostURL, hostName, DELIMITER, 0);		// new Forst

	uint32_t start = strlen(req);
	pos = start;

	char* crc = &req[pos];
	addParameter(&req[pos], "CHECKSUM", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
	addParameter(&req[pos], fileName);

	count = Store::antData(&req[pos], addrRecordStart, addrRecordEnd, allRecords); // pridani obsahu souboru, vraci pocet zaznamu
  addrRecordConfirm = addrRecordStart % EEPROM_TOP;
  addrRecordStart = (addrRecordStart + count) % EEPROM_TOP;
  countSum += count;

  unsigned char* hash=MD5::make_hash(&req[pos]);
  char *md5str = MD5::make_digest(hash, 16);
	memcpy(crc + 103, md5str, strlen(md5str));
  free(md5str);
  free(hash);

	pos = strlen(req);
	addEnd(&req[pos]);

	pos -= start;
  sprintf(&req[start - 9], "%05d", pos);
	req[start-4] = '\r';

	return req;	
}

char* WifiComm::requestAnt(char* req) {
	char fileName[100];
  char trep[17];
	char date[7];
	const char* prefixAll[] = {"inc", "inc", "inc", "res", "dmp"};
	const char* prefix = prefixAll[allRecords];

	DateTime dateTimeEvent(Store::event.time + SECONDS_FROM_1970_TO_2000 + SECONDS_FROM_2000_TO_2020);
	DateTime dateTimeNow(rtc.getTime() + SECONDS_FROM_1970_TO_2000 + SECONDS_FROM_2000_TO_2020);
	DateTime* dateTime = (allRecords != 4) ? &dateTimeEvent : &dateTimeNow;

	sprintf(trep, "ToePunch%08d", Store::config.sn);
  sprintf(date, "%02d%02d%02d", dateTime->year() % 100, dateTime->month(), dateTime->day());

	char ident[] = "Tx";
	ident[1] = Store::station.type == 'C' ? 'P' : Store::station.type;

	sprintf(fileName, 
		"%s.%s.%d..%s%d.%s.UTF-8.txt", 
		prefix, date, Store::event.id & 0xFFFFFF, ident, Store::station.id, trep);

	sprintf(req,
	  "POST /%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: ESP32\r\nContent-Type: multipart/form-data; boundary=\"%s\"\r\nContent-Length: %05d\r\n\r\n",
		hostURL, hostName, DELIMITER, 0);		// new Forst

	// Nahrada parametru v URL
	strreplace(req, "$U", Store::auth.userName);
	strreplace(req, "$P", Store::auth.password);
	strreplace(req, "$E", Store::event.id);
	strreplace(req, "$R", Store::event.id);
	strreplace(req, "$D", date);
	strreplace(req, "$T", ident);
	strreplace(req, "$N", Store::station.id);
	strreplace(req, "$I", trep);

	uint32_t start = strlen(req);
	pos = start;

	addParameter(&req[pos], "TOEPUNCH_ID", trep);
	addParameter(&req[pos], "VERSION", VERSION);
	addParameter(&req[pos], "EVENT_ID", Store::event.id & 0xFFFFFF);
	addParameter(&req[pos], "RACE_ID", Store::event.id & 0xFFFFFF);
	addParameter(&req[pos], "EVENT_DATE", date);
	addParameter(&req[pos], "RACE_DATE", date);
	addParameter(&req[pos], "STATION_NUM", Store::station.id);
	addParameter(&req[pos], "TOE_ADDRESS", addrRecordStart % EEPROM_TOP);
	char* crc = &req[pos];
	addParameter(&req[pos], "CHECKSUM", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
	addParameter(&req[pos], fileName);

	uint32_t time = rtc.getTime();
	char s[] = "hh:mm:ss";
	sprintf(fileName, "#;%d;%s;%s +%02d00;\r\n", countSum / MAX_RECORDS, VERSION, rtc.printTime(s, time), Store::config.timeZone);		
	strcpy(&req[pos], fileName);
	count = Store::antData(&req[pos+strlen(fileName)], addrRecordStart, addrRecordEnd, allRecords); // pridani obsahu souboru, vraci pocet zaznamu
  addrRecordConfirm = addrRecordStart % EEPROM_TOP;
  addrRecordStart = (addrRecordStart + count) % EEPROM_TOP;
  countSum += count;

	MD5_CTX context;
	unsigned char * hash = (unsigned char *) malloc(16);
	MD5::MD5Init(&context);
	MD5::MD5Update(&context, Store::auth.password, strlen(Store::auth.password));
	MD5::MD5Update(&context, trep, strlen(trep));
	MD5::MD5Update(&context, &req[pos], strlen(&req[pos]));
	MD5::MD5Final(hash, &context);
  char *md5str = MD5::make_digest(hash, 16);
	memcpy(crc + 103, md5str, strlen(md5str));
  free(md5str);
  free(hash);

	pos = strlen(req);
	addEnd(&req[pos]);

	pos -= start;
  sprintf(&req[start - 9], "%05d", pos);
	req[start-4] = '\r';

	return req;	
}

char* WifiComm::requestPreoresultat(char* req) {
	TData d;
  Store::readData(addrRecordStart, &d, allRecords);
 	uint8_t s = d.time % 60;
	uint8_t m = d.time / 60 % 60;
	uint8_t h = d.time / 3600 % 24;

  addrRecordConfirm = addrRecordStart % EEPROM_TOP;
  addrRecordStart = (addrRecordStart + 1) % EEPROM_TOP;
  countSum++;

  // http://preoresultat.se/input/ArduinoUpload.php?user=furucz&pass=xxx&c_id=100649&data=1;;;38;010643
	sprintf(req, 
		"GET /%s?user=%s&pass=%s&c_id=%d&data=%d;;;%d;%02d%02d%02d HTTP/1.1\nHost: %s\n\n",
		hostURL,
		Store::auth.userName, Store::auth.password,
		d.idEvent, d.idCompetitor, d.SI, h, m, s,
		hostName);

	return req;	
}

void WifiComm::sendToServer(AsyncClient* client) {
//  DEBUG_PRINTF("Space: %d CanSend: %d\r\n", client->space(), client->canSend());
//	DEBUG_PRINTLN("Send request\r\n");

	if (client->space() > 4096 && client->canSend()) { // Proverit jde to jen do 101 -> nastaveni Apache
		if (Store::server.code == PREO) {
			requestPreoresultat(request);
		} else if (Store::server.code == ANT) {
			requestAnt(request);
		} else {
			requestRaw(request);
		}
		DEBUG_PRINT(request); // Vypis dotazu zdrzuje
		client->write(request);
		state = DATA_SEND;
	} else {
		error = 1;
		Serial.println("Space error");
		state = SERVER_CONNECTED; // ukonceni spojeni
	}
}

int idx;

void addValue(char* s, const char* text) {
	strcpy(s, text);
	idx += strlen(s);
}

void addValue(char* s, const char* name, const char* value) {
	sprintf(s, name, value);
	idx += strlen(s);
}

void addValue(char* s, const char* name, uint32_t value) {
	sprintf(s, name, value);
	idx += strlen(s);
}

void addValue(char* s, const char* name, uint8_t* value) {
	sprintf(s, name, value[0], value[1], value[2], value[3]);
	idx += strlen(s);
}

void addValue(char* s, const char* name, int8_t value) {
	sprintf(s, name, value);
	idx += strlen(s);
}

void addValue(char* s, const char* name, float value) {
	sprintf(s, name, value);
	idx += strlen(s);
}

void WifiComm::sendStatus(AsyncClient* client) {
  DEBUG_PRINTF("Space: %d CanSend: %d\r\n", client->space(), client->canSend());
	DEBUG_PRINTLN("Send request\r\n");

	if (client->space() > 4096 && client->canSend()) { 
		char tmp[4];
	  info.format(tmp, Store::station.id, Store::station.type);

		idx = 0;
		addValue(&request[idx], "GET /%s", Store::status.hostURL);

		if (!allRecords) {
			addValue(&request[idx], "?sn=%d", Store::config.sn);
			addValue(&request[idx], "&batt=%4.2f", voltage);
			addValue(&request[idx], "&idStation=%s", tmp);
			addValue(&request[idx], "&idEvent=%d", Store::event.id);
			addValue(&request[idx], "&idRace=%d", Store::event.id);
			addValue(&request[idx], "&time=%d", rtc.getTime());
			addValue(&request[idx], "&zone=");
			addValue(&request[idx], Store::config.timeZone > 0 ? "%2B" : "%2D");
			addValue(&request[idx], "%02d", (int8_t)abs(Store::config.timeZone));
			addValue(&request[idx], "%02d", (int8_t)0);
			addValue(&request[idx], "&records=%d", (Store::addrActual - Store::addrBase) % EEPROM_TOP);
			addValue(&request[idx], "&counter=%d", (uint32_t)Store::eepromCounter);
			addValue(&request[idx], "&ssid=%s", Store::wifi.ssid);
			addValue(&request[idx], "&ip=%d.%d.%d.%d", Store::server.IP);
			addValue(&request[idx], "&port=%d", (uint32_t)Store::server.port);
			addValue(&request[idx], "&host=%s", Store::server.hostName);
			char s[512];
			urlencode(s, Store::server.hostURL);
			addValue(&request[idx], "&url=%s", s);
			addValue(&request[idx], "&upload=%d", (uint32_t)Store::server.upload);
			addValue(&request[idx], "&code=%d", (uint32_t)Store::server.code);
			addValue(&request[idx], "&ver=%s", VERSION);
			addValue(&request[idx], "&health=%d", (uint32_t)(Store::status.act != 0));
			addValue(&request[idx], "/%d", (uint32_t)(Store::status.pas != 0));
		} else {
			addValue(&request[idx], "?TIME_NTP_STARTED=%d", infoNTP.started);
			addValue(&request[idx], "&TIME_NTP_FINISHED=%d", infoNTP.finished);
			addValue(&request[idx], "&TIME_NOW_APPLIED=%d", infoNTP.applied);
			addValue(&request[idx], "&TIME_RESET=%d", infoNTP.reset);
		}
		addValue(&request[idx], " HTTP/1.1\r\nHost: %s\r\n\r\n", statusName);

		DEBUG_PRINT(request); 
		client->write(request);
		state = DATA_SEND;
	} else {
		error = 1;
		Serial.println("Space error");
		state = DATA_RECEIVED; // ukonceni spojeni
	}
}


/* event callbacks */
void WifiComm::onData(void* arg, AsyncClient* client, void *data, size_t len) {
	DEBUG_PRINTF("\r\nData received from %s \r\n", client->remoteIP().toString().c_str());
	DEBUG_PRINTF("===\r\n%s\r\n===\r\n", (char*)data);

	uint8_t* p = (uint8_t*)data;
	uint8_t i;
	for (i = len-3; p[i] != '\r' && i > 0; i--);
	i++;
	#ifdef DEBUG
	  Serial.write(&p[i], len-i);
	#endif	

  if (!statusUpload) { // neni odesilani stavu
		// nastav status dat jako odeslane, ??? doplnit test
		if (Store::server.code == PREO)  { // Doplnit kontrolu potvrzeni dat serverem
			if (strpos((char*)data, "Done!") != NOT_FOUND) {
				if (!allRecords) { 
					Store::confirmData(addrRecordConfirm); 
				}
				DEBUG_PRINTLN("\r\n1 record uploaded ok");
				char tmp[16];
				sprintf(tmp, "%d records", countSum);
				info.showAsync(tmp, "uploaded", ASYNC_TIMEOUT);
			} else {
				Serial.println("\r\nData upload error");
				info.showAsync("Data", "upload error", ASYNC_TIMEOUT);
				error = 1;
			}
			state = DATA_RECEIVED;
		} else if ((Store::server.code == ANT || Store::server.code == RAW) && strpos((char*)data, "ANT:") != NOT_FOUND) {
			if (strpos((char*)data, "ANT: OK") != NOT_FOUND) {
				if (!allRecords) Store::confirmData(addrRecordConfirm, count); 
				DEBUG_PRINTF("\r\n%d records uploaded ok\r\n", count);
				char tmp[16];
				sprintf(tmp, "%d records", countSum);
				info.showAsync(tmp, "uploaded", ASYNC_TIMEOUT);
			} else {
				Serial.println("\r\nData upload error");
				info.showAsync("Data", "upload error", ASYNC_TIMEOUT);
				error = 1;
			}
			state = DATA_RECEIVED;
		}
	} else { // Status upload
		if (strpos((char*)data, "TOE: OK") != NOT_FOUND) {
			error = 0;
			info.showAsync("Status", "uploaded", ASYNC_TIMEOUT);
		} 
		if (strpos((char*)data, "0\r\n\r\n") != NOT_FOUND) { // posledni chunk
			state = DATA_RECEIVED;	
		}   
		if (strpos((char*)data, "404 Not Found") != NOT_FOUND) { 
			state = DATA_RECEIVED;	
		}   
		
	}
}

void WifiComm::onConnect(void* arg, AsyncClient* client) {
	if (!statusUpload)
		Serial.printf("client has been connected to %s on port %d\r\n", hostIP.toString().c_str(), hostPort);
	else
		Serial.printf("client has been connected to %s on port %d\r\n", IPAddress(Store::status.IP).toString().c_str(), 80);		
	state = SERVER_CONNECTED;
}

void WifiComm::onDisconnect(void* arg, AsyncClient* client) {
	Serial.printf("\r\nTime for communication: %ld\r\n", millis()-timeComm);
	Serial.print("Client has been disconnected\r\n");
	wifiPowerDown();
}

void WifiComm::onTimeout(void* arg, AsyncClient* client, uint32_t time) {
	Serial.printf("TIME OUT!!! %d\r\n", time);
	// info na displej
}

void WifiComm::wifiPowerDown() {
	state = IDLE;
	if (aClient != NULL) {
  	delete aClient;
		aClient = NULL;
	}
	if (WiFi.status() == WL_CONNECTED) 	WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  btStop();
  adc_power_off();
  esp_wifi_stop(); // Espressiv bug - pokud neni zapnute Wifi, tak crash
  esp_bt_controller_disable();
	Serial.println("Wifi power down\r\n");
}

void WifiComm::onError(void* arg, AsyncClient* client, int error) {
	Serial.printf("\r\nError !! %s\r\n", client->errorToString(error));
	info.showAsync("Server", "error", ASYNC_TIMEOUT);
}

bool WifiComm::ntp(uint32_t ip, char* url) {
	if (ip == 0) {
		if (!getIPfromDNS(&ip, url)) return 0;
	} else {
		if (!connectToWifi()) return 0;
	}

	WiFiUDP ntpUDP;
	NTPClient timeClient(ntpUDP, IPAddress(ip));
	
	infoNTP.started = rtc.getTime(); // ladeni F
	char tmp[16];
	info.show("Connecting", info.ipToString(tmp, ip));
  timeClient.begin();
  int count = 5;
	while (!timeClient.forceUpdate() && count > 0) count--;
	if(count > 0) {
		uint32_t t = timeClient.getEpochTime();
		infoNTP.finished = rtc.getTime(); // ladeni F
  	infoNTP.applied = t - (SECONDS_FROM_1970_TO_2000 + SECONDS_FROM_2000_TO_2020 - Store::config.timeZone*3600); // ladeni F
  	rtc.adjustTime(t, 1); 
		infoNTP.reset = rtc.getTime(); // ladeni F
		Serial.print(" RTC adjusted to: ");
		char s[] = "DD.MM.YYYY hh:mm:ss";
		Serial.println(rtc.printTime(s, t, 1));
	} else {
		char tmp2[16];
		info.show("NTP error", tmp, info.ipToString(tmp2, WiFi.localIP()));
		Serial.println(" NTP error");
	}
	timeClient.end();
	wifiPowerDown();
	return (count > 0);
}

void WifiComm::addParameter(char* s, const char* name, const char* value) {
	sprintf(s, "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\nContent-Type: text/plain; charset=UTF-8\r\n\r\n%s\r\n", DELIMITER, name, value);
	pos += strlen(s);
}

void WifiComm::addParameter(char* s, const char* name, uint32_t value) {
	sprintf(s, "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\nContent-Type: text/plain; charset=UTF-8\r\n\r\n%d\r\n", DELIMITER, name, value);
	pos += strlen(s);
}

void WifiComm::addParameter(char* s, const char* fileName) {
	sprintf(s, "--%s\r\nContent-Disposition: form-data; name=\"RESULTS\"; filename=\"%s\"\r\nContent-Type: text/plain; charset=UTF-8\r\nContent-Transfer-Encoding: binary\r\n\r\n", DELIMITER, fileName);
	pos += strlen(s);
}

void WifiComm::addEnd(char* s) {
	sprintf(s, "\r\n--%s--\r\n", DELIMITER);
	pos += strlen(s);
}

int WifiComm::checkForUpdates(char* server) {

	if (!connectToWifi()) return -1;

  int result = 0;
  String fwUrlBase = (String)"http://" + server;
  String fwVersionURL = fwUrlBase;
  fwVersionURL.concat("firmware.version");
  Serial.println("Checking for firmware updates.");
  Serial.print("Firmware version URL: ");
  Serial.println(fwVersionURL);

	char tmp[16];
	memcpy(tmp, server, 15);
	tmp[15] = 0;
	info.show("Connecting", tmp);

	WiFiClient client;  /// FOR ESP32 HTTP FOTA UPDATE ///
  HTTPClient httpClient;
  httpClient.begin(client, fwVersionURL);
  int httpCode = httpClient.GET();

  if (httpCode == 200) {
    String newFWVersion = httpClient.getString();
    Serial.print("Current firmware version: ");
    Serial.println(FW_VERSION);
    Serial.print("Available firmware version: ");
    Serial.println(newFWVersion);
    int newVersion = newFWVersion.toInt();
    if (newVersion != FW_VERSION) {
      Serial.println("Preparing to update");
      String fwImageURL = fwUrlBase + (String)newVersion;      
      fwImageURL.concat(".bin");
      Serial.print("Image for upload: ");
      Serial.println(fwImageURL);
			info.show("Downloading", tmp);
      t_httpUpdate_return ret = httpUpdate.update(client, fwImageURL); /// FOR ESP32 HTTP FOTA
      switch(ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str()); /// FOR ESP32
          result = -1;
          break;
        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          result = -2;
          break;
        case HTTP_UPDATE_OK:
          Serial.println("HTTP_UPDATE_OK");
          break;
      }
    }
    else {
      Serial.println( "Already on latest version" );
      result = 1;
    }
  }
  else {
    Serial.print("Firmware version check failed, got HTTP response code ");
    Serial.println(httpCode);
    result = -3;
  }
  Serial.println("HTTP client ended.");
  httpClient.end();
	wifiPowerDown();
  return result;
}

bool WifiComm::connectToWifi() {
  if (!connect()) return 0;
  info.show("Wifi", "connecting", Store::wifi.ssid);
  state = IDLE;
  while (WiFi.status() != WL_CONNECTED) {
		if (millis() - timeOut > 10000) {
			info.show("Wifi", "connection failed", Store::wifi.ssid);
		  wifiPowerDown();
			return 0;
		}
    Serial.print('.');
    delay(500);
  }
	Serial.println(" connected");
	return 1;
}

bool WifiComm::getIPfromDNS(uint32_t* ip, const char* host) {

	char tmp[128];
	int i = strpos(host, "/");
	if (i > 0) {
		memcpy(tmp, host, i);
		tmp[i] = 0;
	} else {
		strcpy(tmp, host);
	}

	if (!connectToWifi()) return 0;

  info.show("DNS", "for", tmp);

	IPAddress result;
	int res = WiFi.hostByName(tmp, result);
  if (res == 1) {
		char s[16];
		*ip = result;
		info.show("IP address", tmp, info.ipToString(s, (uint8_t*)ip));
		Serial.print("Ip address: ");
		Serial.println(s);
  } else {
		Serial.print("DNS error for ");
		Serial.println(tmp);
		info.show("DNS", "don't resolve", tmp);
  } 
	if (!res) wifiPowerDown();
	return res == 1;
}

