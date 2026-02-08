import bzapi
from .. import Flag

flag = Flag("low_gravity")

flag.name = {
    "en": "Low Gravity",
    "fr": "Faible gravité",
    "de": "Niedrige Schwerkraft",
    "es": "Baja gravedad",
    "pt": "Baixa gravidade",
    "ja": "低重力",
    "ru": "Низкая гравитация",
    "zh": "低重力",
}

flag.description = {}

flag.params = {}

flag.register()
