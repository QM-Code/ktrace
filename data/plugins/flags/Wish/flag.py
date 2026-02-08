import bzapi
from .. import Flag

flag = Flag("wish")

flag.name = {
    "en": "Wish",
    "fr": "Souhait",
    "de": "Wunsch",
    "es": "Deseo",
    "pt": "Desejo",
    "ja": "願い",
    "ru": "Желание",
    "zh": "愿望",
}

flag.description = {}

flag.params = {}

flag.register()
