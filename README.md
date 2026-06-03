# ESPHome NMEA0183 Clients

Two lightweight ESPHome external components for **ESP32** that push sensor data
as NMEA0183 `$IIXDR` sentences to a marine network — one over **TCP**, one over
**UDP**.

Designed for the **OBP60** maritime display (and any other device that accepts
NMEA0183 over the network), so you can feed e.g. Victron, battery or temperature
data straight onto your boat's instrument bus.

Both components compile under the **ESP-IDF** and **Arduino** frameworks.

---

## Components at a glance

| | `nmea_tcp_client` | `nmea_udp_client` |
|---|---|---|
| Transport | TCP (connection-oriented) | UDP (connectionless) |
| Connection | Persistent, auto-reconnect every 5 s | None — one datagram per sentence |
| Delivery | Reliable, ordered | Fire-and-forget (may drop) |
| Server needed | A TCP **server** must listen (e.g. OBP60 on 10110) | A UDP **listener** must be bound to the port |
| Blocking | Connect runs in a background FreeRTOS task — never blocks the main loop | Never blocks |
| Best for | A single known receiver, guaranteed delivery | Broadcast-style feeds, multiple/loose receivers, lowest overhead |

Both expose the exact same call from a lambda:

```cpp
id(nmea_out).send_xdr("U", 12.45, "V", "BatVoltage");
```

---

## NMEA0183 XDR sentence format

```
$IIXDR,<type>,<value>,<unit>,<name>*<checksum>\r\n
```

Example output:

```
$IIXDR,U,12.450,V,BatVoltage*3A
$IIXDR,I,-3.200,A,BatCurrent*1F
```

The checksum is the XOR of all characters between `$` and `*`. The value is
formatted with 3 decimal places.

Common transducer types:

| Type | Meaning      | Typical unit |
|------|--------------|--------------|
| `U`  | Voltage      | `V`          |
| `I`  | Current      | `A`          |
| `C`  | Temperature  | `C`          |
| `P`  | Pressure (or percent, your choice) | `P` |
| `G`  | Generic      | e.g. `AH`    |
| `W`  | Power        | `W`          |
| `E`  | Energy       | `KWH`        |

> `name` is a free-text transducer label (e.g. `BatVoltage`) that the receiver
> can map to a display field.

---

## Installation

Reference this repository as an ESPHome external component and pick the
component(s) you want:

```yaml
external_components:
  - source: github://MultivitaminJuice/esphome-nmea-tcp-client
    components: [nmea_tcp_client]      # and/or nmea_udp_client
```

If you cloned the repo and your YAML sits next to the `components/` folder, you
can also use a local source (this is what [`example.yaml`](example.yaml) does):

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [nmea_tcp_client, nmea_udp_client]
```

---

## Configuration

### TCP client

```yaml
nmea_tcp_client:
  id: nmea_out
  host: 192.168.2.5     # IP of the TCP server (e.g. OBP60)
  port: 10110           # NMEA0183 TCP port
```

### UDP client

```yaml
nmea_udp_client:
  id: nmea_out
  host: 192.168.2.255   # unicast IP or broadcast address
  port: 10110
```

| Option | Required | Description |
|--------|----------|-------------|
| `id`   | yes      | ID used to call `send_xdr()` from lambdas |
| `host` | yes      | Target IP address (TCP server, or UDP unicast/broadcast) |
| `port` | yes      | Target port (commonly `10110` for NMEA0183) |

Both depend on `wifi` and use setup priority `AFTER_WIFI`.

---

## Sending data

Call `send_xdr()` from any lambda — typically in a sensor's `on_value`:

```yaml
sensor:
  - platform: template
    name: "Battery Voltage"
    lambda: 'return 12.45;'
    update_interval: 5s
    on_value:
      then:
        - lambda: id(nmea_out).send_xdr("U", x, "V", "BatVoltage");
```

Or on a fixed interval:

```yaml
interval:
  - interval: 5s
    then:
      - lambda: |-
          id(nmea_out).send_xdr("U", 12.45, "V", "BatVoltage");
          id(nmea_out).send_xdr("I", -3.2,  "A", "BatCurrent");
```

See [`example.yaml`](example.yaml) for a complete, minimal device config.

---

## Behaviour notes

**`nmea_tcp_client`**
- Connects to `host:port` on startup.
- Reconnects automatically every 5 s if the link drops.
- The connect (DNS + TCP handshake) runs in a dedicated FreeRTOS task, so a slow
  or unreachable server never blocks the ESPHome main loop or triggers the
  watchdog.
- `send_xdr()` drops the sentence (with a warning log) while not connected — no
  crash, no blocking.

**`nmea_udp_client`**
- Opens a UDP socket, sends one datagram, closes it — per sentence.
- No connection state and no delivery guarantee; ideal for broadcast-style feeds.

The decoded/forwarded values are logged at `INFO`, so you can confirm output in
the ESPHome log.

---

## Repository layout

```
components/
  nmea_tcp_client/
    __init__.py          # ESPHome config schema
    nmea_tcp_client.h    # C++ implementation (TCP, FreeRTOS connect task)
  nmea_udp_client/
    __init__.py          # ESPHome config schema
    nmea_udp_client.h    # C++ implementation (UDP, lwip sockets)
example.yaml             # complete minimal example (local source)
secrets.yaml.example     # template — copy to secrets.yaml and fill in
```

---

## License

MIT — see [LICENSE](LICENSE).
