#pragma once
#include "esphome/core/component.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <stdio.h>
#include <string.h>

namespace esphome {
namespace nmea_udp_client {

class NmeaUdpClientComponent : public Component {
 public:
  void set_host(const char *host) { host_ = host; }
  void set_port(uint16_t port) { port_ = port; }

  void setup() override {}
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void send_xdr(const char *type, double value, const char *unit, const char *name) {
    char sentence[120];
    snprintf(sentence, sizeof(sentence), "IIXDR,%s,%.3f,%s,%s", type, value, unit, name);
    send_nmea(sentence);
  }

 protected:
  const char *host_{nullptr};
  uint16_t port_{10110};

  void send_nmea(const char *sentence) {
    // Checksum berechnen
    uint8_t checksum = 0;
    for (const char *p = sentence; *p; p++) checksum ^= (uint8_t)*p;

    char full[150];
    snprintf(full, sizeof(full), "$%s*%02X\r\n", sentence, checksum);

    int sock = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return;

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port_);
    dest.sin_addr.s_addr = ipaddr_addr(host_);

    lwip_sendto(sock, full, strlen(full), 0, (struct sockaddr *)&dest, sizeof(dest));
    lwip_close(sock);
  }
};

}  // namespace nmea_udp_client
}  // namespace esphome
