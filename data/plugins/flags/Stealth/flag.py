import bzapi
from .. import Flag

flag = Flag("stealth")

flag.name = {
    "en": "Stealth",
    "fr": "Furtivité",
    "de": "Tarnung",
    "es": "Sigilo",
    "pt": "Furtividade",
    "ja": "ステルス",
    "ru": "Скрытность",
    "zh": "潜行",
}

flag.description = {}

flag.params = {}

flag.register()
