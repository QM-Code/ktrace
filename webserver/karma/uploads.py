import io
import os
import secrets

from karma import config

try:
    from PIL import Image
except ImportError:  # Optional dependency
    Image = None


def _uploads_dir():
    settings = config.get_config()
    community_dir = config.get_community_dir() or config.get_config_dir()
    uploads_dir = config.require_setting(settings, "uploads.upload_directory")
    if os.path.isabs(uploads_dir):
        return uploads_dir
    return os.path.normpath(os.path.join(community_dir, uploads_dir))


def _screenshot_settings(settings):
    screenshots = config.require_setting(settings, "uploads.screenshots")
    limits = config.require_setting(settings, "uploads.screenshots.limits")
    thumb = config.require_setting(settings, "uploads.screenshots.thumbnail")
    full = config.require_setting(settings, "uploads.screenshots.full")
    return {
        "min_bytes": int(config.require_setting(limits, "min_bytes", "config.json uploads.screenshots.limits")),
        "max_bytes": int(config.require_setting(limits, "max_bytes", "config.json uploads.screenshots.limits")),
        "min_width": int(config.require_setting(limits, "min_width", "config.json uploads.screenshots.limits")),
        "min_height": int(config.require_setting(limits, "min_height", "config.json uploads.screenshots.limits")),
        "max_width": int(config.require_setting(limits, "max_width", "config.json uploads.screenshots.limits")),
        "max_height": int(config.require_setting(limits, "max_height", "config.json uploads.screenshots.limits")),
        "thumb_width": int(config.require_setting(thumb, "width", "config.json uploads.screenshots.thumbnail")),
        "thumb_height": int(config.require_setting(thumb, "height", "config.json uploads.screenshots.thumbnail")),
        "full_width": int(config.require_setting(full, "width", "config.json uploads.screenshots.full")),
        "full_height": int(config.require_setting(full, "height", "config.json uploads.screenshots.full")),
    }


def ensure_upload_dir():
    os.makedirs(_uploads_dir(), exist_ok=True)


def _save_bytes(filename, data):
    path = os.path.join(_uploads_dir(), filename)
    with open(path, "wb") as handle:
        handle.write(data)
    return filename


def _screenshot_names(token):
    return {
        "original": f"{token}_original.jpg",
        "full": f"{token}_full.jpg",
        "thumb": f"{token}_thumb.jpg",
    }


def screenshot_urls(token):
    if not token:
        return {}
    names = _screenshot_names(token)
    return {key: f"/uploads/{value}" for key, value in names.items()}


def _scale_image(image, max_width, max_height):
    scaled = image.copy()
    scaled.thumbnail((max_width, max_height), Image.LANCZOS)
    return scaled


def _msg(key, **values):
    template = config.ui_text(f"messages.uploads.{key}", "config.json ui_text.messages.uploads")
    return config.format_text(template, **values)


def handle_upload(file_item):
    settings = config.get_config()
    limits = _screenshot_settings(settings)
    min_bytes = limits["min_bytes"]
    max_bytes = limits["max_bytes"]
    min_width = limits["min_width"]
    min_height = limits["min_height"]
    max_width = limits["max_width"]
    max_height = limits["max_height"]
    thumb_width = limits["thumb_width"]
    thumb_height = limits["thumb_height"]
    full_width = limits["full_width"]
    full_height = limits["full_height"]

    if Image is None:
        return None, _msg("missing_pillow")

    data = file_item.file.read()
    if not data:
        return None, _msg("empty_file")
    if len(data) < min_bytes:
        return None, _msg("file_too_small", min_bytes=min_bytes)
    if len(data) > max_bytes:
        return None, _msg("file_too_large", max_bytes=max_bytes)

    try:
        image = Image.open(io.BytesIO(data))
        image.load()
    except Exception:
        return None, _msg("invalid_image")

    width, height = image.size
    if width < min_width or height < min_height:
        return None, _msg("resolution_too_small")
    if width > max_width or height > max_height:
        return None, _msg("resolution_too_large")

    ensure_upload_dir()
    token = secrets.token_hex(12)
    names = _screenshot_names(token)

    original = image.convert("RGB")
    original_bytes = io.BytesIO()
    original.save(original_bytes, format="JPEG", quality=90, optimize=True)
    _save_bytes(names["original"], original_bytes.getvalue())

    full_img = _scale_image(original, full_width, full_height)
    full_bytes = io.BytesIO()
    full_img.save(full_bytes, format="JPEG", quality=85, optimize=True)
    _save_bytes(names["full"], full_bytes.getvalue())

    thumb_img = _scale_image(original, thumb_width, thumb_height)
    thumb_bytes = io.BytesIO()
    thumb_img.save(thumb_bytes, format="JPEG", quality=80, optimize=True)
    _save_bytes(names["thumb"], thumb_bytes.getvalue())

    return {"id": token}, None
