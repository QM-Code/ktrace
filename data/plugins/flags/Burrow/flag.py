import bzapi
from .. import Flag

flag = Flag("burrow")

flag.name = {
    "en": "Burrow",
    "fr": "Terrier",
    "de": "Eingraben",
    "es": "Cavar",
    "pt": "Escavar",
    "ja": "潜行",
    "ru": "Зарывание",
    "zh": "掩埋",
}

flag.description = {
    "en": "Lowers tank into ground, making you more difficult to hit but able to be steamrolled.",
    "fr": "Enfonce le char dans le sol, le rendant plus difficile à toucher mais pouvant être écrasé.",
    "de": "Senkt den Panzer in den Boden, macht ihn schwerer zu treffen, aber kann überfahren werden.",
    "es": "Hunde el tanque en el suelo, haciéndolo más difícil de acertar pero vulnerable a ser aplastado.",
    "pt": "Rebaixa o tanque no chão, tornando-o mais difícil de atingir, mas passível de ser atropelado.",
    "ja": "戦車を地面に沈め、当てにくくしますが、轢かれる可能性があります。",
    "ru": "Опускает танк в землю, делая его труднее для попадания, но его можно раздавить.",
    "zh": "使坦克下沉到地面，更难命中，但可被碾压。",
}

flag.params = {}

flag.register()
