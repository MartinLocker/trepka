#ifndef WIFICOMM_H
#define WIFICOMM_H

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "data.h"
#include "system.h"

#include <HTTPClient.h> /// FOR ESP32 HTTP FOTA UPDATE ///
#include <HTTPUpdate.h> /// FOR ESP32 HTTP FOTA UPDATE ///
#include <WiFiClient.h> /// FOR ESP32 HTTP FOTA UPDATE ///

#include "version.h"

#define ASYNC_TIMEOUT 3000
#define WIFI_TIMEOUT  5000
//#define TIME_TO_STATUS_SLEEP  (15 * 60)  /* Cas aktivity odesilani statusu po poslednim razeni v sekundach */
#define NEXT_STATUS_NEVER (3600 * 24 * 365) /* "Nekonecny" cas pro odeslani dalsiho statusu */

#define DELIMITER "AaB03xy"


class WifiComm {
  public:
    typedef enum {
      IDLE,
      WIFI_START,
      WIFI_CONNECTING,
      WIFI_CONNECTED,
      SERVER_CONNECTING,
      SERVER_CONNECTED,
      DATA_SEND,
      DATA_RECEIVED,
      ERROR
    } TState;

    static char* ssid;
    static char* password;
    static IPAddress localIP;
    static IPAddress gateway;
    static IPAddress subnet;
    
    static uint16_t  hostPort;
    static IPAddress hostIP;
    static char* hostName;
    static char* hostURL;
    static char* statusName;

    static uint8_t error;

    static bool statusUpload; // Priznak odesilani statusu

    static float voltage;
    
    WifiComm();
    void init();
    void start(uint8_t all = 0);  // zahaji prenos 
    bool process();
    bool connect();
    bool ntp(uint32_t ip, char* url);
    int  checkForUpdates(char* server);
    bool connectToWifi();
    bool getIPfromDNS(uint32_t* ip, const char* host);
    
    static void wifiPowerDown();
    static void sendToServer(AsyncClient* client);
    static void onConnect(void* arg, AsyncClient* client);
    static void onData(void* arg, AsyncClient* client, void *data, size_t len);
    static void onDisconnect(void* arg, AsyncClient* client);
    static void onTimeout(void* arg, AsyncClient* client, uint32_t time);
    static void onError(void* arg, AsyncClient* client, int error);

    static void sendStatus(AsyncClient* client);

    static TState state;  

    void   setStatus(uint32_t);
    bool   setNextStatus(uint32_t);

  private:
    uint32_t statusStamp = 0; // Cas posledniho odeslani statusu
    bool     statusActive = 0; // priznak activniho odesilani statusu
    uint32_t statusActivePeriod  = NEXT_STATUS_NEVER; // perioda odesilani statusu pri aktivnim stavu
    uint32_t statusPassivePeriod = NEXT_STATUS_NEVER; // perioda odesilani statusu pri pasivnim stavu

    uint32_t timeOut;
    TData  data;
    static uint16_t addrRecordConfirm;
    static uint16_t addrRecordStart;
    static uint16_t addrRecordEnd;
    static uint8_t  allRecords; // priznak odesilani vsech dat
    uint32_t lastTime;
    static uint32_t count; // pocet odeslanych zaznamu
    static uint32_t countSum; // pocet odeslanych zaznamu
        
    static char request[16384];	// pole pro sestavovani dotazu
    static uint32_t pos; // pozice v poli znaku pri sestavovani dotazu

    static void addParameter(char* s, const char* name, uint32_t value);
    static void addParameter(char* s, const char* name, const char* value);
    static void addParameter(char* s, const char* fileName);
    static void addEnd(char* s);

    static char* requestRaw(char* req);
    static char* requestAnt(char* req);
    static char* requestPreoresultat(char* req);
};

extern WifiComm comm; 

#endif
