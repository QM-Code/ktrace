import bzapi
from .. import Flag

flag = Flag("identify")

flag.name = {
    "en": "Identify",
    "fr": "Identifier",
    "de": "Identifizieren",
    "es": "Identificar",
    "pt": "Identificar",
    "ja": "識別",
    "ru": "Идентификация",
    "zh": "识别",
}

flag.description = {}

flag.params = {}

flag.register()
