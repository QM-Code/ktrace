import bzapi
from .. import Flag

flag = Flag("colorblindness")

flag.name = {
    "en": "Colorblindness",
    "fr": "Daltonisme",
    "de": "Farbenblindheit",
    "es": "Daltonismo",
    "pt": "Daltonismo",
    "ja": "色覚異常",
    "ru": "Дальтонизм",
    "zh": "色盲",
}

flag.description = {
    "en": "All tanks appear as the same color.",
    "fr": "Tous les chars apparaissent de la même couleur.",
    "de": "Alle Panzer erscheinen in derselben Farbe.",
    "es": "Todos los tanques aparecen del mismo color.",
    "pt": "Todos os tanques aparecem da mesma cor.",
    "ja": "すべての戦車が同じ色に見えます。",
    "ru": "Все танки выглядят одного цвета.",
    "zh": "所有坦克看起来都是同一种颜色。",
}

flag.params = {}

flag.register()
