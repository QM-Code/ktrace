import bzapi
from .. import Flag

flag = Flag("jamming")

flag.name = {
    "en": "Jamming",
    "fr": "Brouillage",
    "de": "Störung",
    "es": "Interferencia",
    "pt": "Interferência",
    "ja": "妨害",
    "ru": "Помехи",
    "zh": "干扰",
}

flag.description = {}

flag.params = {}

flag.register()
