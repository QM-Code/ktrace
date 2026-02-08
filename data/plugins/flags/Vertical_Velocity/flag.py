import bzapi
from .. import Flag

flag = Flag("vertical_velocity")

flag.name = {
    "en": "Vertical Velocity",
    "fr": "Vitesse verticale",
    "de": "Vertikale Geschwindigkeit",
    "es": "Velocidad vertical",
    "pt": "Velocidade vertical",
    "ja": "垂直速度",
    "ru": "Вертикальная скорость",
    "zh": "垂直速度",
}

flag.description = {}

flag.params = {}

flag.register()
