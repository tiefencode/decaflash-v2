#pragma once

#include <Arduino.h>
#include <esp_now.h>

namespace decaflash::node {

struct RadioRepairResult {
  bool ok = false;
  // Error codes of the steps, for the serial log. ESP_FAIL where a step did not run.
  esp_err_t wifiStop = ESP_FAIL;
  esp_err_t wifiDeinit = ESP_FAIL;
  esp_err_t wifiInit = ESP_FAIL;
  esp_err_t wifiStart = ESP_FAIL;
  esp_err_t wifiSetChannel = ESP_FAIL;
  esp_err_t espNowInit = ESP_FAIL;
  esp_err_t addPeer = ESP_FAIL;
  esp_err_t registerReceive = ESP_FAIL;
};

// Stage 1: re-initialise ESP-NOW on channel 1 and register the receive handler.
RadioRepairResult reinitEspNow(esp_now_recv_cb_t receiveHandler);

// Stage 2: shut ESP-NOW and the Wi-Fi driver down and run the boot sequence again.
// "Wi-Fi" is the radio driver ESP-NOW runs on; no network is joined.
RadioRepairResult restartWifi(esp_now_recv_cb_t receiveHandler);

// Primary Wi-Fi channel of the radio, 0 if it cannot be read.
uint8_t currentRadioChannel();

}  // namespace decaflash::node
