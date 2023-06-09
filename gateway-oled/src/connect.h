#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include "bars.h"
#include "private.h"

void initVariant();
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len);
void handleRoot();
void handleNotFound();
void setupConnect();
void loopConnect();
