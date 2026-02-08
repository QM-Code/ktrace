import bzapi
from .. import Flag

flag = Flag("blindness")

flag.name = {
    "en": "Blindness",
    "fr": "Cécité",
    "de": "Blindheit",
    "es": "Ceguera",
    "pt": "Cegueira",
    "ja": "失明",
    "ru": "Слепота",
    "zh": "失明",
}

flag.description = {
    "en": "Renders the viewport inoperable.",
    "fr": "Rend la vue inutilisable.",
    "de": "Macht das Sichtfeld unbrauchbar.",
    "es": "Vuelve inoperable la vista.",
    "pt": "Torna a visão inutilizável.",
    "ja": "視界を使用不能にします。",
    "ru": "Делает обзор непригодным для использования.",
    "zh": "使视野无法使用。",
}

flag.params = {}

flag.register()
