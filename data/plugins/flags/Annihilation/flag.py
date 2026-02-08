import bzapi
from .. import Flag

flag = Flag("annihilation")

flag.name = {
    "en": "Annihilation",
    "fr": "Annihilation",
    "de": "Vernichtung",
    "es": "Aniquilación",
    "pt": "Aniquilação",
    "ja": "全滅",
    "ru": "Уничтожение",
    "zh": "歼灭",
}

flag.description = {
    "en": "Kills everyone on all teams if you kill one other person.",
    "fr": "Tue tout le monde dans toutes les équipes si vous tuez une autre personne.",
    "de": "Tötet alle in allen Teams, wenn du eine andere Person tötest.",
    "es": "Mata a todos en todos los equipos si matas a otra persona.",
    "pt": "Mata todos em todas as equipes se você matar outra pessoa.",
    "ja": "誰か1人を倒すと、全チームの全員を倒します。",
    "ru": "Убивает всех во всех командах, если вы убьете другого человека.",
    "zh": "如果你击杀一个人，则杀死所有队伍的所有人。",
}

flag.params = {}

flag.register()
