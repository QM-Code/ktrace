import html
from html.parser import HTMLParser
from urllib.parse import urlparse

from karma import webhttp

try:
    import markdown as _markdown  # type: ignore
except Exception:  # Optional dependency
    _markdown = None


_ALLOWED_TAGS = {
    "p",
    "br",
    "strong",
    "em",
    "ul",
    "ol",
    "li",
    "code",
    "pre",
    "blockquote",
    "h1",
    "h2",
    "h3",
    "h4",
    "a",
}

_ALLOWED_ATTRS = {
    "a": {"href", "title"},
}


class _Sanitizer(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=False)
        self._parts = []

    def handle_starttag(self, tag, attrs):
        if tag not in _ALLOWED_TAGS:
            return
        allowed = _ALLOWED_ATTRS.get(tag, set())
        rendered_attrs = []
        for key, value in attrs:
            if key not in allowed:
                continue
            if key == "href":
                if not _safe_href(value):
                    continue
            if value is None:
                rendered_attrs.append(key)
            else:
                rendered_attrs.append(f'{key}="{html.escape(value, quote=True)}"')
        attrs_text = ""
        if rendered_attrs:
            attrs_text = " " + " ".join(rendered_attrs)
        self._parts.append(f"<{tag}{attrs_text}>")

    def handle_endtag(self, tag):
        if tag in _ALLOWED_TAGS:
            self._parts.append(f"</{tag}>")

    def handle_data(self, data):
        self._parts.append(html.escape(data))

    def handle_entityref(self, name):
        self._parts.append(f"&{name};")

    def handle_charref(self, name):
        self._parts.append(f"&#{name};")

    def get_html(self):
        return "".join(self._parts)


def _safe_href(value):
    if not value:
        return False
    parsed = urlparse(value)
    if parsed.scheme in ("http", "https", "mailto"):
        return True
    if parsed.scheme == "" and (value.startswith("/") or value.startswith("#")):
        return True
    return False


def _fallback_render(text):
    if not text:
        return ""
    escaped = webhttp.html_escape(text)
    parts = [part for part in escaped.split("\n\n") if part != ""]
    if not parts:
        return ""
    paragraphs = []
    for part in parts:
        paragraphs.append(f"<p>{part.replace('\\n', '<br>')}</p>")
    return "".join(paragraphs)


def render_markdown(text):
    if text is None:
        return ""
    raw = text.strip()
    if not raw:
        return ""
    if _markdown is None:
        return _fallback_render(raw)
    html_text = _markdown.markdown(raw, extensions=["extra", "sane_lists"])
    sanitizer = _Sanitizer()
    sanitizer.feed(html_text)
    return sanitizer.get_html()
