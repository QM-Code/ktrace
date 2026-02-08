import bzapi
from .. import Flag

flag = Flag("airstrike")

flag.name = {
    "en": "Airstrike",
    "fr": "Frappe aérienne",
    "de": "Luftangriff",
    "es": "Ataque aéreo",
    "pt": "Ataque aéreo",
    "ja": "空爆",
    "ru": "Авианалет",
    "zh": "空袭",
}

flag.description = {
    "en": "Calls in a series of shockwaves from above.",
    "fr": "Appelle une série d'ondes de choc venant d'en haut.",
    "de": "Ruft eine Serie von Schockwellen von oben herbei.",
    "es": "Llama una serie de ondas de choque desde arriba.",
    "pt": "Chama uma série de ondas de choque vindas de cima.",
    "ja": "上空から連続する衝撃波を呼び出します。",
    "ru": "Вызывает серию ударных волн сверху.",
    "zh": "从上方召唤一系列冲击波。",
}

flag.params = {}

flag.register()
