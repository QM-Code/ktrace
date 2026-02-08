import bzapi
from .. import Flag

flag = Flag("trigger_happy")

flag.name = {
    "en": "Trigger Happy",
    "fr": "Gâchette facile",
    "de": "Schießwütig",
    "es": "Gatillo fácil",
    "pt": "Gatilho fácil",
    "ja": "引き金が軽い",
    "ru": "Легкий на спуск",
    "zh": "爱乱开枪",
}

flag.description = {}

flag.params = {}

flag.register()
