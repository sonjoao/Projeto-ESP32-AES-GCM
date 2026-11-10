#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_random.h>
#include <mbedtls/gcm.h>
#include <cstring>
#include "secrets.h"

#if !defined(DEVICE_ROLE_SENDER) && !defined(DEVICE_ROLE_RECEIVER)
#error "Selecione DEVICE_ROLE_SENDER ou DEVICE_ROLE_RECEIVER no platformio.ini"
#endif
#if defined(DEVICE_ROLE_SENDER) && defined(DEVICE_ROLE_RECEIVER)
#error "Selecione apenas uma funcao por placa"
#endif

namespace {
constexpr uint8_t WIFI_CHANNEL = 1;
constexpr size_t MAX_MESSAGE_SIZE = 180;
constexpr uint32_t PACKET_MAGIC = 0x45535032; // ESP2
constexpr uint8_t PACKET_VERSION = 1;
constexpr uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#pragma pack(push, 1)
struct SecurePacket {
  uint32_t magic;
  uint8_t version;
  uint8_t nonce[12];
  uint16_t length;
  uint8_t ciphertext[MAX_MESSAGE_SIZE];
  uint8_t tag[16];
};
#pragma pack(pop)

bool configureRadio() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    Serial.println("ERRO: nao foi possivel configurar o canal Wi-Fi.");
    return false;
  }
  return true;
}

bool configureEspNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERRO: inicializacao do ESP-NOW falhou.");
    return false;
  }
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, BROADCAST_MAC, sizeof(BROADCAST_MAC));
  peer.channel = WIFI_CHANNEL;
  peer.encrypt = false; // A confidencialidade e autenticidade ficam no AES-GCM.
  if (!esp_now_is_peer_exist(BROADCAST_MAC) && esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("ERRO: nao foi possivel registrar o destino broadcast.");
    return false;
  }
  return true;
}

bool encryptMessage(const uint8_t *plain, size_t length, SecurePacket &packet) {
  if (length > MAX_MESSAGE_SIZE) return false;
  packet.magic = PACKET_MAGIC;
  packet.version = PACKET_VERSION;
  packet.length = static_cast<uint16_t>(length);
  esp_fill_random(packet.nonce, sizeof(packet.nonce));

  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  int rc = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, AES_GCM_KEY, 256);
  if (rc == 0) {
    rc = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, length,
      packet.nonce, sizeof(packet.nonce), nullptr, 0, plain, packet.ciphertext,
      sizeof(packet.tag), packet.tag);
  }
  mbedtls_gcm_free(&ctx);
  if (rc != 0) {
    Serial.printf("ERRO: AES-GCM falhou (codigo %d).\n", rc);
    return false;
  }
  return true;
}

bool authenticateAndDecrypt(const SecurePacket &packet, uint8_t *plain) {
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  int rc = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, AES_GCM_KEY, 256);
  if (rc == 0) {
    rc = mbedtls_gcm_auth_decrypt(&ctx, packet.length, packet.nonce,
      sizeof(packet.nonce), nullptr, 0, packet.tag, sizeof(packet.tag),
      packet.ciphertext, plain);
  }
  mbedtls_gcm_free(&ctx);
  return rc == 0;
}

#if defined(DEVICE_ROLE_RECEIVER)
void onDataReceived(const uint8_t *mac, const uint8_t *data, int length) {
  if (length != static_cast<int>(sizeof(SecurePacket))) {
    Serial.printf("Pacote ignorado: tamanho inesperado (%d bytes).\n", length);
    return;
  }
  SecurePacket packet{};
  memcpy(&packet, data, sizeof(packet));
  if (packet.magic != PACKET_MAGIC || packet.version != PACKET_VERSION ||
      packet.length > MAX_MESSAGE_SIZE) {
    Serial.println("Pacote ignorado: cabecalho invalido.");
    return;
  }

  uint8_t plain[MAX_MESSAGE_SIZE + 1]{};
  if (!authenticateAndDecrypt(packet, plain)) {
    Serial.println("FALHA: autenticacao AES-GCM invalida; mensagem descartada.");
    return;
  }
  plain[packet.length] = '\0';
  Serial.printf("Mensagem autenticada de %02X:%02X:%02X:%02X:%02X:%02X: %s\n",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
    reinterpret_cast<const char *>(plain));
}
#endif
}

void setup() {
  Serial.begin(115200);
  delay(500);
#if defined(DEVICE_ROLE_SENDER)
  Serial.println("ESP32 remetente | digite uma mensagem e pressione Enter (max. 180 bytes)");
#else
  Serial.println("ESP32 destinataria | aguardando mensagens ESP-NOW autenticadas...");
#endif
  if (!configureRadio() || !configureEspNow()) {
    Serial.println("Inicializacao interrompida.");
    return;
  }
#if defined(DEVICE_ROLE_RECEIVER)
  esp_now_register_recv_cb(onDataReceived);
#endif
  Serial.print("MAC desta placa: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
#if defined(DEVICE_ROLE_SENDER)
  static char input[MAX_MESSAGE_SIZE + 1];
  if (Serial.available() > 0) {
    size_t length = Serial.readBytesUntil('\n', input, MAX_MESSAGE_SIZE + 1);
    while (length > 0 && (input[length - 1] == '\r' || input[length - 1] == '\n')) --length;
    if (length == 0) return;
    if (length > MAX_MESSAGE_SIZE) {
      Serial.println("Mensagem longa demais; limite: 180 bytes.");
      return;
    }
    SecurePacket packet{};
    if (!encryptMessage(reinterpret_cast<const uint8_t *>(input), length, packet)) return;
    esp_err_t result = esp_now_send(BROADCAST_MAC,
      reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
    if (result == ESP_OK) Serial.println("Mensagem cifrada enviada por ESP-NOW.");
    else Serial.printf("Falha no envio (codigo %d).\n", result);
  }
#endif
  delay(5);
}
