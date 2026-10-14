from esphome import automation
import esphome.codegen as cg
from esphome.components import climate, remote_transmitter, sensor, uart
from esphome.components.climate import ClimateMode, ClimatePreset, ClimateSwingMode
from esphome.components.remote_base import CONF_TRANSMITTER_ID
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTOCONF,
    CONF_BEEPER,
    CONF_CUSTOM_FAN_MODES,
    CONF_CUSTOM_PRESETS,
    CONF_ID,
    CONF_NUM_ATTEMPTS,
    CONF_OUTDOOR_TEMPERATURE,
    CONF_PERIOD,
    CONF_SUPPORTED_MODES,
    CONF_SUPPORTED_PRESETS,
    CONF_SUPPORTED_SWING_MODES,
    CONF_TEMPERATURE,
    CONF_TIMEOUT,
    CONF_USE_FAHRENHEIT,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    ICON_POWER,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_WATT,
)
from esphome.core import CORE, coroutine

CODEOWNERS = ["@dudanov"]
DEPENDENCIES = ["climate", "uart"]
AUTO_LOAD = ["sensor"]
CONF_POWER_USAGE = "power_usage"
CONF_HUMIDITY_SETPOINT = "humidity_setpoint"
midea_ac_ns = cg.esphome_ns.namespace("midea").namespace("ac")
AirConditioner = midea_ac_ns.class_("AirConditioner", climate.Climate, cg.Component)
Capabilities = midea_ac_ns.namespace("Constants")


def templatize(value):
    if isinstance(value, cv.Schema):
        value = value.schema
    ret = {}
    for key, val in value.items():
        ret[key] = cv.templatable(val)
    return cv.Schema(ret)


def register_action(name, type_, schema):
    validator = templatize(schema).extend(MIDEA_ACTION_BASE_SCHEMA)
    registerer = automation.register_action(
        f"midea_ac.{name}", type_, validator, synchronous=True
    )

    def decorator(func):
        async def new_func(config, action_id, template_arg, args):
            ac_ = await cg.get_variable(config[CONF_ID])
            var = cg.new_Pvariable(action_id, template_arg)
            cg.add(var.set_parent(ac_))
            await coroutine(func)(var, config, args)
            return var

        return registerer(new_func)

    return decorator


ALLOWED_CLIMATE_MODES = {
    "HEAT_COOL": ClimateMode.CLIMATE_MODE_HEAT_COOL,
    "COOL": ClimateMode.CLIMATE_MODE_COOL,
    "HEAT": ClimateMode.CLIMATE_MODE_HEAT,
    "DRY": ClimateMode.CLIMATE_MODE_DRY,
    "FAN_ONLY": ClimateMode.CLIMATE_MODE_FAN_ONLY,
}

ALLOWED_CLIMATE_PRESETS = {
    "ECO": ClimatePreset.CLIMATE_PRESET_ECO,
    "BOOST": ClimatePreset.CLIMATE_PRESET_BOOST,
    "SLEEP": ClimatePreset.CLIMATE_PRESET_SLEEP,
}

ALLOWED_CLIMATE_SWING_MODES = {
    "BOTH": ClimateSwingMode.CLIMATE_SWING_BOTH,
    "VERTICAL": ClimateSwingMode.CLIMATE_SWING_VERTICAL,
    "HORIZONTAL": ClimateSwingMode.CLIMATE_SWING_HORIZONTAL,
}

CUSTOM_FAN_MODES = {
    "SILENT": Capabilities.SILENT,
    "TURBO": Capabilities.TURBO,
}

CUSTOM_PRESETS = {
    "FREEZE_PROTECTION": Capabilities.FREEZE_PROTECTION,
}

validate_modes = cv.enum(ALLOWED_CLIMATE_MODES, upper=True)
validate_presets = cv.enum(ALLOWED_CLIMATE_PRESETS, upper=True)
validate_swing_modes = cv.enum(ALLOWED_CLIMATE_SWING_MODES, upper=True)
validate_custom_fan_modes = cv.enum(CUSTOM_FAN_MODES, upper=True)
validate_custom_presets = cv.enum(CUSTOM_PRESETS, upper=True)

DIAGNOSTIC_SENSORS = {
    "compressor_frequency_raw": (0, sensor.sensor_schema(accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT)),
    "target_frequency_raw": (1, sensor.sensor_schema(accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT)),
    "compressor_current_raw": (2, sensor.sensor_schema(accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT)),
    "compressor_voltage_raw": (3, sensor.sensor_schema(accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT)),
    "indoor_ambient": (4, sensor.sensor_schema(accuracy_decimals=1, unit_of_measurement='°C', device_class=DEVICE_CLASS_TEMPERATURE, state_class=STATE_CLASS_MEASUREMENT)),
    "indoor_coil": (5, sensor.sensor_schema(accuracy_decimals=1, unit_of_measurement='°C', device_class=DEVICE_CLASS_TEMPERATURE, state_class=STATE_CLASS_MEASUREMENT)),
    "outdoor_coil": (6, sensor.sensor_schema(accuracy_decimals=1, unit_of_measurement='°C', device_class=DEVICE_CLASS_TEMPERATURE, state_class=STATE_CLASS_MEASUREMENT)),
    "outdoor_ambient": (7, sensor.sensor_schema(accuracy_decimals=1, unit_of_measurement='°C', device_class=DEVICE_CLASS_TEMPERATURE, state_class=STATE_CLASS_MEASUREMENT)),
    "discharge_pipe": (8, sensor.sensor_schema(accuracy_decimals=1, unit_of_measurement='°C', device_class=DEVICE_CLASS_TEMPERATURE, state_class=STATE_CLASS_MEASUREMENT)),
    "indoor_fan": (9, sensor.sensor_schema(accuracy_decimals=0, unit_of_measurement='rpm', state_class=STATE_CLASS_MEASUREMENT)),
    "target_indoor_fan": (10, sensor.sensor_schema(accuracy_decimals=0, unit_of_measurement='rpm', state_class=STATE_CLASS_MEASUREMENT)),
    "outdoor_fan": (11, sensor.sensor_schema(accuracy_decimals=0, unit_of_measurement='rpm', state_class=STATE_CLASS_MEASUREMENT)),
    "indoor_humidity": (12, sensor.sensor_schema(accuracy_decimals=0, unit_of_measurement='%', device_class=DEVICE_CLASS_HUMIDITY, state_class=STATE_CLASS_MEASUREMENT)),
    "outdoor_silent_raw": (13, sensor.sensor_schema(accuracy_decimals=0)),
    "display_raw": (14, sensor.sensor_schema(accuracy_decimals=0)),
    "self_clean_raw": (15, sensor.sensor_schema(accuracy_decimals=0)),
}

CONFIG_SCHEMA = cv.All(
    climate.climate_schema(AirConditioner)
    .extend(
        {
            cv.Optional(CONF_PERIOD, default="1s"): cv.time_period,
            cv.Optional(CONF_TIMEOUT, default="2s"): cv.time_period,
            cv.Optional(CONF_NUM_ATTEMPTS, default=3): cv.int_range(min=1, max=5),
            cv.OnlyWith(CONF_TRANSMITTER_ID, "remote_transmitter"): cv.use_id(
                remote_transmitter.RemoteTransmitterComponent
            ),
            cv.Optional(CONF_BEEPER, default=False): cv.boolean,
            cv.Optional(CONF_AUTOCONF, default=True): cv.boolean,
            cv.Optional(CONF_SUPPORTED_MODES): cv.ensure_list(validate_modes),
            cv.Optional(CONF_SUPPORTED_SWING_MODES): cv.ensure_list(
                validate_swing_modes
            ),
            cv.Optional(CONF_SUPPORTED_PRESETS): cv.ensure_list(validate_presets),
            cv.Optional(CONF_CUSTOM_PRESETS): cv.ensure_list(validate_custom_presets),
            cv.Optional(CONF_CUSTOM_FAN_MODES): cv.ensure_list(
                validate_custom_fan_modes
            ),
            cv.Optional(CONF_OUTDOOR_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional("vane_position_raw"): sensor.sensor_schema(accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT),
            cv.Optional("vane_position_interval", default="1s"): cv.positive_time_period_milliseconds,
            cv.Optional("indoor_fan_interval"): cv.positive_time_period_milliseconds,
            cv.Optional("energy_probe", default=False): cv.boolean,
            cv.Optional("extended_diagnostics"): cv.Schema({cv.Optional(key): schema for key, (_, schema) in DIAGNOSTIC_SENSORS.items()}),
            cv.Optional("compressor_power"): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional("compressor_power_interval", default="30s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_POWER_USAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_POWER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY_SETPOINT): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.only_on(
        [
            PLATFORM_ESP32,
            PLATFORM_ESP8266,
        ]
    ),
)

# Actions
FollowMeAction = midea_ac_ns.class_("FollowMeAction", automation.Action)
DisplayToggleAction = midea_ac_ns.class_("DisplayToggleAction", automation.Action)
SwingStepAction = midea_ac_ns.class_("SwingStepAction", automation.Action)
BeeperOnAction = midea_ac_ns.class_("BeeperOnAction", automation.Action)
BeeperOffAction = midea_ac_ns.class_("BeeperOffAction", automation.Action)
PowerOnAction = midea_ac_ns.class_("PowerOnAction", automation.Action)
PowerOffAction = midea_ac_ns.class_("PowerOffAction", automation.Action)
PowerToggleAction = midea_ac_ns.class_("PowerToggleAction", automation.Action)

MIDEA_ACTION_BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(AirConditioner),
    }
)

# FollowMe action
MIDEA_FOLLOW_ME_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TEMPERATURE): cv.templatable(cv.temperature),
        cv.Optional(CONF_USE_FAHRENHEIT, default=False): cv.templatable(cv.boolean),
        cv.Optional(CONF_BEEPER, default=False): cv.templatable(cv.boolean),
    }
)


@register_action("follow_me", FollowMeAction, MIDEA_FOLLOW_ME_SCHEMA)
async def follow_me_to_code(var, config, args):
    template_ = await cg.templatable(config[CONF_BEEPER], args, cg.bool_)
    cg.add(var.set_beeper(template_))
    template_ = await cg.templatable(config[CONF_USE_FAHRENHEIT], args, cg.bool_)
    cg.add(var.set_use_fahrenheit(template_))
    template_ = await cg.templatable(config[CONF_TEMPERATURE], args, cg.float_)
    cg.add(var.set_temperature(template_))


QueryVaneAction = midea_ac_ns.class_("QueryVaneAction", automation.Action)


@register_action("query_vane", QueryVaneAction, cv.Schema({}))
async def query_vane_to_code(var, config, args):
    pass


# Toggle Display action
@register_action(
    "display_toggle",
    DisplayToggleAction,
    cv.Schema({}),
)
async def display_toggle_to_code(var, config, args):
    pass


# Swing Step action
@register_action(
    "swing_step",
    SwingStepAction,
    cv.Schema({}),
)
async def swing_step_to_code(var, config, args):
    pass


# Beeper On action
@register_action(
    "beeper_on",
    BeeperOnAction,
    cv.Schema({}),
)
async def beeper_on_to_code(var, config, args):
    pass


# Beeper Off action
@register_action(
    "beeper_off",
    BeeperOffAction,
    cv.Schema({}),
)
async def beeper_off_to_code(var, config, args):
    pass


# Power On action
@register_action(
    "power_on",
    PowerOnAction,
    cv.Schema({}),
)
async def power_on_to_code(var, config, args):
    pass


# Power Off action
@register_action(
    "power_off",
    PowerOffAction,
    cv.Schema({}),
)
async def power_off_to_code(var, config, args):
    pass


# Power Toggle action
@register_action(
    "power_toggle",
    PowerToggleAction,
    cv.Schema({}),
)
async def power_inv_to_code(var, config, args):
    pass


FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "midea", baud_rate=9600, require_rx=True, require_tx=True
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_period(config[CONF_PERIOD].total_milliseconds))
    cg.add(var.set_response_timeout(config[CONF_TIMEOUT].total_milliseconds))
    cg.add(var.set_request_attempts(config[CONF_NUM_ATTEMPTS]))
    if CONF_TRANSMITTER_ID in config:
        cg.add_define("USE_REMOTE_TRANSMITTER")
        transmitter_ = await cg.get_variable(config[CONF_TRANSMITTER_ID])
        cg.add(var.set_transmitter(transmitter_))
    cg.add(var.set_beeper_feedback(config[CONF_BEEPER]))
    cg.add(var.set_autoconf(config[CONF_AUTOCONF]))
    if CONF_SUPPORTED_MODES in config:
        cg.add(var.set_supported_modes(config[CONF_SUPPORTED_MODES]))
    if CONF_SUPPORTED_SWING_MODES in config:
        cg.add(var.set_supported_swing_modes(config[CONF_SUPPORTED_SWING_MODES]))
    if CONF_SUPPORTED_PRESETS in config:
        cg.add(var.set_supported_presets(config[CONF_SUPPORTED_PRESETS]))
    if CONF_CUSTOM_PRESETS in config:
        cg.add(var.set_custom_presets(config[CONF_CUSTOM_PRESETS]))
    if CONF_CUSTOM_FAN_MODES in config:
        cg.add(var.set_custom_fan_modes(config[CONF_CUSTOM_FAN_MODES]))
    if CONF_OUTDOOR_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_OUTDOOR_TEMPERATURE])
        cg.add(var.set_outdoor_temperature_sensor(sens))
    if "vane_position_raw" in config:
        sens = await sensor.new_sensor(config["vane_position_raw"])
        cg.add(var.set_vane_position_sensor(sens))
        cg.add(var.set_vane_position_interval(config["vane_position_interval"].total_milliseconds))
    if "indoor_fan_interval" in config:
        cg.add(var.set_indoor_fan_interval(config["indoor_fan_interval"].total_milliseconds))
    cg.add(var.set_energy_probe(config["energy_probe"]))
    for key, sensor_config in config.get("extended_diagnostics", {}).items():
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_diagnostic_sensor(DIAGNOSTIC_SENSORS[key][0], sens))
    if "compressor_power" in config:
        sens = await sensor.new_sensor(config["compressor_power"])
        cg.add(var.set_compressor_power_sensor(sens))
        cg.add(var.set_compressor_power_interval(config["compressor_power_interval"].total_milliseconds))
    if CONF_POWER_USAGE in config:
        sens = await sensor.new_sensor(config[CONF_POWER_USAGE])
        cg.add(var.set_power_sensor(sens))
    if CONF_HUMIDITY_SETPOINT in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY_SETPOINT])
        cg.add(var.set_humidity_setpoint_sensor(sens))
    # MideaUART uses the Arduino WiFi API for the network-notify frame
    # (WiFi auto-enables Network via dependency mapping). On ESP-IDF the
    # library talks to esp_wifi directly, so no library entry is needed.
    if CORE.is_esp32 and CORE.using_arduino:
        cg.add_library("WiFi", None)
    # Using the repository until a release containing ESP-IDF support is published
    cg.add_library(
        name="MideaUART",
        version=None,
        repository="https://github.com/dudanov/MideaUART.git#eeea6c3e9b4474f067054592b435be1c4e466815",
    )


QueryCompressorPowerAction = midea_ac_ns.class_("QueryCompressorPowerAction", automation.Action)


@register_action("query_compressor_power", QueryCompressorPowerAction, cv.Schema({}))
async def query_compressor_power_to_code(var, config, args):
    pass


SetVaneAction = midea_ac_ns.class_("SetVaneAction", automation.Action)


@register_action("set_vane", SetVaneAction, cv.Schema({cv.Required("position"): cv.All(cv.int_, cv.one_of(1, 25, 50, 75, 100))}))
async def set_vane_to_code(var, config, args):
    value = await cg.templatable(config["position"], args, cg.uint8)
    cg.add(var.set_position(value))


SetExtendedAction = midea_ac_ns.class_("SetExtendedAction", automation.Action)


@register_action("set_extended", SetExtendedAction, cv.Schema({
    cv.Required("feature"): cv.one_of("outdoor_silent", "display", "self_clean"),
    cv.Required("enabled"): cv.boolean,
}))
async def set_extended_to_code(var, config, args):
    tags = {"outdoor_silent": 0xCD, "display": 0x17, "self_clean": 0x39}
    values = {"outdoor_silent": 3, "display": 100, "self_clean": 1}
    feature = config["feature"]
    cg.add(var.set_tag(await cg.templatable(tags[feature], args, cg.uint16)))
    # Keep feature static: a template could otherwise choose a tag without a matching value map.
    enabled = config["enabled"]
    if not isinstance(enabled, bool):
        raise cv.Invalid("enabled must be a literal true/false for this experimental action")
    cg.add(var.set_value(await cg.templatable(values[feature] if enabled else 0, args, cg.uint8)))


DisplayToggleUARTOnceAction = midea_ac_ns.class_("DisplayToggleUARTOnceAction", automation.Action)


@register_action("display_toggle_uart_once", DisplayToggleUARTOnceAction, cv.Schema({}))
async def display_toggle_uart_once_to_code(var, config, args):
    pass
