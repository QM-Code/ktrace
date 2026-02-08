import bzapi
from .. import Flag

flag = Flag("bouncy")

flag.name = {
    "en": "Bouncy",
    "fr": "Rebondissant",
    "de": "Hüpfend",
    "es": "Rebotante",
    "pt": "Saltitante",
    "ja": "バウンド",
    "ru": "Прыгучий",
    "zh": "弹跳",
}

flag.description = {
    "en": "Makes tank jump uncontrollably.",
    "fr": "Fait sauter le char de façon incontrôlable.",
    "de": "Lässt den Panzer unkontrolliert springen.",
    "es": "Hace que el tanque salte sin control.",
    "pt": "Faz o tanque pular de forma incontrolável.",
    "ja": "戦車が制御不能に跳ねます。",
    "ru": "Заставляет танк бесконтрольно подпрыгивать.",
    "zh": "使坦克失控地跳跃。",
}

flag.params = {}

flag.register()
