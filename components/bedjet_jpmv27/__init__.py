from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, time
from esphome.const import (
    CONF_ID,
    CONF_ON_UPDATE,
    CONF_RECEIVE_TIMEOUT,
    CONF_TIME_ID,
    CONF_TRIGGER_ID,
)

CODEOWNERS = ["@jhansche"]
DEPENDENCIES = ["ble_client"]
MULTI_CONF = True
CONF_BEDJET_ID = "bedjet_id"

bedjet_ns = cg.esphome_ns.namespace("bedjet")
BedJetHub = bedjet_ns.class_("BedJetHub", ble_client.BLEClientNode, cg.PollingComponent)

# Triggers
UpdateTrigger =bedjet_ns.class_("UpdateTrigger", automation.Trigger.template())

CONFIG_SCHEMA = (
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(BedJetHub),
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(
                CONF_RECEIVE_TIMEOUT, default="0s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ON_UPDATE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(UpdateTrigger),
                }
            ),

        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("15s"))
)

BEDJET_CLIENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BEDJET_ID): cv.use_id(BedJetHub),
    }
)


async def register_bedjet_child(var, config):
    parent = await cg.get_variable(config[CONF_BEDJET_ID])
    cg.add(parent.register_child(var))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
    if time_id := config.get(CONF_TIME_ID):
        time_ = await cg.get_variable(time_id)
        cg.add(var.set_time_id(time_))
    if (receive_timeout := config.get(CONF_RECEIVE_TIMEOUT)) is not None:
        cg.add(var.set_status_timeout(receive_timeout))
    for conf in config.get(CONF_ON_UPDATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_onupdate_trigger(trigger))
        await automation.build_automation(trigger, [], conf)

