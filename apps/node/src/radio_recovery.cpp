#include "radio_recovery.h"

#include <esp_wifi.h>

#include "espnow_transport.h"

namespace decaflash::node {

RadioRepairResult reinitEspNow(esp_now_recv_cb_t receiveHandler) {
  RadioRepairResult result;
  const auto recovery = espnow_transport::recoverEspNow();
  result.wifiInit = recovery.wifiInit;
  result.wifiStart = recovery.wifiStart;
  result.wifiSetChannel = recovery.wifiSetChannel;
  result.espNowInit = recovery.espNowInit;
  result.addPeer = recovery.peer.addPeer;
  if (!recovery.ok()) {
    return result;
  }

  // esp_now_deinit() dropped the handler registered at boot.
  result.registerReceive = esp_now_register_recv_cb(receiveHandler);
  result.ok = result.registerReceive == ESP_OK;
  return result;
}

RadioRepairResult restartWifi(esp_now_recv_cb_t receiveHandler) {
  RadioRepairResult result;
  // Tear down as far as it goes; each step may fail if the layer is not up.
  esp_now_deinit();
  result.wifiStop = esp_wifi_stop();
  result.wifiDeinit = esp_wifi_deinit();

  const auto init = espnow_transport::initEspNow();
  result.wifiInit = init.wifiInit;
  result.wifiStart = init.wifiStart;
  result.wifiSetChannel = init.wifiSetChannel;
  result.espNowInit = init.espNowInit;
  if (!init.ok()) {
    return result;
  }

  const auto peer = espnow_transport::ensureBroadcastPeer();
  result.addPeer = peer.addPeer;
  if (!peer.ok()) {
    return result;
  }

  result.registerReceive = esp_now_register_recv_cb(receiveHandler);
  result.ok = result.registerReceive == ESP_OK;
  return result;
}

uint8_t currentRadioChannel() {
  uint8_t primary = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  if (esp_wifi_get_channel(&primary, &second) != ESP_OK) {
    return 0;
  }
  return primary;
}

}  // namespace decaflash::node
