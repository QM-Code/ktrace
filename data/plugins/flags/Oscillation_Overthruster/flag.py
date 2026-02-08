import bzapi
from .. import Flag

flag = Flag("oscillation_overthruster")

flag.name = {
    "en": "Oscillation Overthruster",
    "fr": "Surpoussée oscillante",
    "de": "Oszillierender Überantrieb",
    "es": "Sobrepropulsor oscilante",
    "pt": "Sobrempropulsor oscilante",
    "ja": "振動オーバースラスター",
    "ru": "Колебательный овертрастер",
    "zh": "振荡超推进器",
}

flag.description = {}

flag.params = {}

flag.register()
