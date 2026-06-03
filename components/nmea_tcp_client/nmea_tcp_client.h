#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include <string>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esphome {
namespace nmea_tcp_client {

static const char *const TAG = "nmea_tcp_client";
static const uint32_t RECONNECT_INTERVAL_MS = 5000;

class NmeaTcpClientComponent : public Component {
 public:
  void set_host(const std::string &host) { host_ = host; }
  void set_port(uint16_t port) { port_ = port; }

  void setup() override {
    ESP_LOGCONFIG(TAG, "Setting up NMEA TCP Client to %s:%u", host_.c_str(), port_);
    // Kick off the background connect task immediately
    schedule_connect_();
  }

  void loop() override {
    // Nothing blocking here — all TCP work happens in connect_task_
  }

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  /**
   * Send an NMEA0183 $IIXDR sentence (called from ESPHome main loop).
   *
   * Example output:  $IIXDR,U,12.450,V,BatVoltage*3A\r\n
   *
   * @param type   Transducer type character, e.g. "U" (voltage), "I" (current)
   * @param value  Measured value
   * @param unit   Unit string, e.g. "V", "A", "C"
   * @param name   Transducer name, e.g. "BatVoltage"
   */
  void send_xdr(const char *type, float value, const char *unit, const char *name) {
    int fd = sockfd_;  // atomic read of int is safe on Xtensa
    if (fd < 0) {
      ESP_LOGW(TAG, "Not connected — dropping XDR for %s", name);
      // Trigger reconnect if not already in progress
      schedule_connect_();
      return;
    }

    // Build sentence body (without $ and *)
    char body[96];
    snprintf(body, sizeof(body), "IIXDR,%s,%.3f,%s,%s", type, value, unit, name);

    // XOR checksum over body
    uint8_t checksum = 0;
    for (size_t i = 0; body[i] != '\0'; i++)
      checksum ^= (uint8_t) body[i];

    char sentence[112];
    int len = snprintf(sentence, sizeof(sentence), "$%s*%02X\r\n", body, checksum);

    ssize_t written = ::send(fd, sentence, len, MSG_DONTWAIT);
    if (written != len) {
      ESP_LOGW(TAG, "Send failed for %s (errno %d) — closing socket", name, errno);
      close_socket_();
      schedule_connect_();
    } else {
      ESP_LOGV(TAG, "Sent: %s", sentence);
    }
  }

 protected:
  // ── Background connect task ───────────────────────────────────────────────

  void schedule_connect_() {
    // Only one connect task at a time
    if (connecting_)
      return;
    uint32_t now = millis();
    if (now - last_reconnect_attempt_ < RECONNECT_INTERVAL_MS)
      return;
    connecting_ = true;
    last_reconnect_attempt_ = now;

    // Pass 'this' as task parameter; task deletes itself when done
    xTaskCreate(
        connect_task_fn_,     // task function
        "nmea_connect",       // name
        4096,                 // stack (bytes)
        this,                 // parameter
        1,                    // priority (low, below main loop)
        nullptr               // handle not needed
    );
  }

  static void connect_task_fn_(void *arg) {
    auto *self = static_cast<NmeaTcpClientComponent *>(arg);
    self->do_connect_();
    self->connecting_ = false;
    vTaskDelete(nullptr);  // delete this task
  }

  void do_connect_() {
    ESP_LOGD(TAG, "Connecting to %s:%u ...", host_.c_str(), port_);

    struct addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", port_);

    struct addrinfo *res = nullptr;
    int err = getaddrinfo(host_.c_str(), port_str, &hints, &res);
    if (err != 0 || res == nullptr) {
      ESP_LOGW(TAG, "DNS lookup for %s failed (%d)", host_.c_str(), err);
      return;
    }

    int fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
      ESP_LOGW(TAG, "socket() failed (errno %d)", errno);
      freeaddrinfo(res);
      return;
    }

    // Set a connect timeout via SO_SNDTIMEO so we don't block forever
    struct timeval tv{};
    tv.tv_sec  = 4;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (::connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
      ESP_LOGW(TAG, "connect() to %s:%u failed (errno %d), retry in %u s",
               host_.c_str(), port_, errno, RECONNECT_INTERVAL_MS / 1000);
      ::close(fd);
      freeaddrinfo(res);
      return;
    }

    freeaddrinfo(res);
    sockfd_ = fd;  // publish to main loop
    ESP_LOGI(TAG, "Connected to %s:%u", host_.c_str(), port_);
  }

  void close_socket_() {
    int fd = sockfd_;
    if (fd >= 0) {
      sockfd_ = -1;
      ::close(fd);
    }
  }

  // ── Member variables ──────────────────────────────────────────────────────
  std::string host_;
  uint16_t port_{10110};
  volatile int sockfd_{-1};      // written by task, read by main loop
  volatile bool connecting_{false};
  uint32_t last_reconnect_attempt_{0};
};

}  // namespace nmea_tcp_client
}  // namespace esphome
