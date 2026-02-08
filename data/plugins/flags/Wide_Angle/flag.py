import bzapi
from .. import Flag

flag = Flag("wide_angle")

flag.name = {
    "en": "Wide Angle",
    "fr": "Grand angle",
    "de": "Weitwinkel",
    "es": "Gran angular",
    "pt": "Grande angular",
    "ja": "広角",
    "ru": "Широкий угол",
    "zh": "广角",
}

flag.description = {}

flag.params = {}

flag.register()
