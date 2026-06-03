import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT

CONF_HOST = "host"

CODEOWNERS = ["@MultivitaminJuice"]
DEPENDENCIES = ["wifi"]
AUTO_LOAD = []

nmea_udp_client_ns = cg.esphome_ns.namespace("nmea_udp_client")
NmeaUdpClientComponent = nmea_udp_client_ns.class_(
    "NmeaUdpClientComponent", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NmeaUdpClientComponent),
        cv.Required(CONF_HOST): cv.string,
        cv.Required(CONF_PORT): cv.port,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_host(config[CONF_HOST]))
    cg.add(var.set_port(config[CONF_PORT]))
