import esphome.codegen as cg
from esphome.components import media_player
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@guillaumebianchin84"]

ikabot_proxy_ns = cg.esphome_ns.namespace("ikabot_proxy_media_player")

IkabotProxyMediaPlayer = ikabot_proxy_ns.class_(
    "IkabotProxyMediaPlayer",
    media_player.MediaPlayer,
    cg.Component,
)

CONFIG_SCHEMA = cv.All(
    media_player.media_player_schema(IkabotProxyMediaPlayer),
    cv.only_on_esp32,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_player.register_media_player(var, config)
