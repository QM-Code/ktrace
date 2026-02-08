import bzapi
from .. import Flag

flag = Flag("wings")

flag.name = {
    "en": "Wings",
    "fr": "Ailes",
    "de": "Flügel",
    "es": "Alas",
    "pt": "Asas",
    "ja": "翼",
    "ru": "Крылья",
    "zh": "翅膀",
}

flag.description = {}

flag.params = {}

flag.register()
