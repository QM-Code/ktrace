import json
import os
import threading


_CONFIG = None
_BASE_CONFIG = None
_COMMUNITY_CONFIG = None
_CONFIG_PATH = None
_COMMUNITY_DIR = None
_LANGUAGE_CONFIGS = {}
_CONFIG_BY_LANGUAGE = {}
_AVAILABLE_LANGUAGES = None
_THREAD_LOCAL = threading.local()
_PORT_OVERRIDE = None


def _find_config_path():
    env_path = os.environ.get("KARMA_CONFIG")
    if env_path:
        return env_path
    base_dir = os.path.dirname(__file__)
    candidates = [
        os.path.join(base_dir, "config.json"),
        os.path.normpath(os.path.join(base_dir, "..", "config.json")),
    ]
    for path in candidates:
        if os.path.isfile(path):
            return path
    raise FileNotFoundError("config.json not found.")


def _load_config():
    global _CONFIG_PATH, _BASE_CONFIG
    config_path = _find_config_path()
    try:
        with open(config_path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
    except json.JSONDecodeError as exc:
        location = f"line {exc.lineno} column {exc.colno}" if exc.lineno and exc.colno else "unknown location"
        raise ValueError(f"[karma] Error: corrupt config.json file ({location}).") from exc
    _CONFIG_PATH = config_path
    _BASE_CONFIG = data
    _reset_language_cache()
    return data


def set_community_dir(path):
    global _COMMUNITY_DIR, _COMMUNITY_CONFIG, _CONFIG
    _COMMUNITY_DIR = path
    _COMMUNITY_CONFIG = None
    _CONFIG = None
    _reset_language_cache()


def set_port_override(port):
    global _PORT_OVERRIDE
    _PORT_OVERRIDE = port


def get_port_override():
    return _PORT_OVERRIDE


def get_community_dir():
    return _COMMUNITY_DIR


def get_community_config_path():
    community_dir = get_community_dir()
    if not community_dir:
        return None
    return os.path.join(community_dir, "config.json")


def _load_community_config():
    global _COMMUNITY_CONFIG
    community_path = get_community_config_path()
    if not community_path:
        _COMMUNITY_CONFIG = None
        return None
    try:
        with open(community_path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
    except FileNotFoundError:
        _COMMUNITY_CONFIG = None
        return None
    except json.JSONDecodeError as exc:
        location = f"line {exc.lineno} column {exc.colno}" if exc.lineno and exc.colno else "unknown location"
        community_dir = get_community_dir()
        if community_dir:
            label = os.path.basename(community_dir.rstrip("/\\")) + "/config.json"
        else:
            label = "community config.json"
        raise ValueError(f"[karma] Error: corrupt {label} file ({location}).") from exc
    _COMMUNITY_CONFIG = data
    _reset_language_cache()
    return data


def _deep_merge(base, override):
    if not isinstance(base, dict) or not isinstance(override, dict):
        return override
    merged = dict(base)
    for key, value in override.items():
        if isinstance(value, dict) and isinstance(merged.get(key), dict):
            merged[key] = _deep_merge(merged[key], value)
        else:
            merged[key] = value
    return merged


def get_base_config():
    return _BASE_CONFIG or _load_config()


def _reset_language_cache():
    global _LANGUAGE_CONFIGS, _CONFIG_BY_LANGUAGE, _AVAILABLE_LANGUAGES
    _LANGUAGE_CONFIGS = {}
    _CONFIG_BY_LANGUAGE = {}
    _AVAILABLE_LANGUAGES = None


def _load_strings_config(language):
    language = normalize_language(language)
    if not language:
        return None
    if language in _LANGUAGE_CONFIGS:
        return _LANGUAGE_CONFIGS[language]
    config_dir = get_config_dir()
    community_dir = get_community_dir()
    base_path = os.path.join(config_dir, "strings", f"{language}.json") if config_dir else None
    community_path = os.path.join(community_dir, "strings", f"{language}.json") if community_dir else None
    base_data = None
    community_data = None
    if base_path:
        try:
            with open(base_path, "r", encoding="utf-8") as handle:
                base_data = json.load(handle)
        except FileNotFoundError:
            base_data = None
        except json.JSONDecodeError as exc:
            location = f"line {exc.lineno} column {exc.colno}" if exc.lineno and exc.colno else "unknown location"
            raise ValueError(f"[karma] Error: corrupt strings/{language}.json file ({location}).") from exc
    if community_path:
        try:
            with open(community_path, "r", encoding="utf-8") as handle:
                community_data = json.load(handle)
        except FileNotFoundError:
            community_data = None
        except json.JSONDecodeError as exc:
            location = f"line {exc.lineno} column {exc.colno}" if exc.lineno and exc.colno else "unknown location"
            raise ValueError(f"[karma] Error: corrupt community strings/{language}.json file ({location}).") from exc
    if base_data is None and community_data is None:
        _LANGUAGE_CONFIGS[language] = None
        return None
    merged = dict(base_data or {})
    if community_data:
        merged = _deep_merge(merged, community_data)
    _LANGUAGE_CONFIGS[language] = merged
    return merged


def get_available_languages():
    global _AVAILABLE_LANGUAGES
    if _AVAILABLE_LANGUAGES is None:
        languages = {"en"}
        for base_dir in (get_config_dir(), get_community_dir()):
            if not base_dir or not os.path.isdir(base_dir):
                continue
            strings_dir = os.path.join(base_dir, "strings")
            if not os.path.isdir(strings_dir):
                continue
            for name in os.listdir(strings_dir):
                if not name.endswith(".json"):
                    continue
                code = name[: -len(".json")].strip().lower()
                if code:
                    languages.add(code)
        _AVAILABLE_LANGUAGES = sorted(languages)
    return list(_AVAILABLE_LANGUAGES)


def normalize_language(language):
    return str(language or "").strip().lower()


def get_default_language():
    base = get_base_config()
    community = _COMMUNITY_CONFIG or _load_community_config() or {}
    language = (community.get("server") or {}).get("language") or require_setting(base, "server.language")
    return normalize_language(language)


def _collect_missing(node, paths, label):
    missing = []
    for path in paths:
        target = node
        parts = path.split(".")
        found = True
        for part in parts:
            if not isinstance(target, dict) or part not in target:
                found = False
                break
            target = target[part]
        if not found or target is None or target == "":
            missing.append(f"{label}: missing {path}")
    return missing


def _load_strings_en():
    config_dir = get_config_dir()
    if not config_dir:
        raise ValueError("[karma] Error: config.json directory not found.")
    path = os.path.join(config_dir, "strings", "en.json")
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return json.load(handle)
    except FileNotFoundError as exc:
        raise ValueError("[karma] Error: missing strings/en.json.") from exc
    except json.JSONDecodeError as exc:
        location = f"line {exc.lineno} column {exc.colno}" if exc.lineno and exc.colno else "unknown location"
        raise ValueError(f"[karma] Error: corrupt strings/en.json file ({location}).") from exc


def validate_startup(settings):
    base = get_base_config()
    required_config_paths = [
        "server.host",
        "server.port",
        "database.database_directory",
        "database.database_file",
        "uploads.upload_directory",
        "uploads.max_request_bytes",
        "heartbeat.timeout_seconds",
        "server.language",
        "pages.servers.overview_max_chars",
        "pages.servers.auto_refresh",
        "pages.servers.auto_refresh_animate",
        "uploads.screenshots.limits",
        "uploads.screenshots.thumbnail",
        "uploads.screenshots.full",
        "debug.heartbeat",
        "debug.auth",
        "debug.disable_browser_language_detection",
        "debug.reset_rate_limits",
        "debug.pretty_print_json",
        "httpserver.threads",
        "rate_limits.login",
        "rate_limits.register",
        "rate_limits.forgot",
        "rate_limits.reset",
        "rate_limits.api_auth",
        "rate_limits.api_user_registered",
        "session_cookie.secure",
        "session_cookie.same_site",
        "session_cookie.http_only",
        "security_headers.content_security_policy",
        "security_headers.referrer_policy",
        "security_headers.x_content_type_options",
        "cache_headers.static",
        "cache_headers.uploads",
        "logging.enabled",
        "logging.level",
        "logging.rotation",
        "logging.max_bytes",
        "logging.backups",
        "logging.utc",
        "logging.dir",
        "logging.server_file",
        "logging.include_user",
        "logging.paths",
    ]
    missing = _collect_missing(base, required_config_paths, "config.json")
    community_required_paths = [
        "server.community_name",
        "server.admin_user",
        "server.session_secret",
    ]
    community = _COMMUNITY_CONFIG or _load_community_config() or {}
    community_dir = get_community_dir()
    if community_dir:
        label = os.path.basename(community_dir.rstrip("/\\")) + "/config.json"
    else:
        label = "community config.json"
    missing.extend(_collect_missing(community, community_required_paths, label))

    strings_en = _load_strings_en()
    required_strings_paths = [
        "meta.direction",
        "languages",
        "ui_text",
        "ui_text.messages",
        "ui_text.labels",
        "ui_text.actions",
        "ui_text.titles",
        "ui_text.sections",
        "ui_text.status",
        "ui_text.errors",
        "ui_text.confirm",
        "ui_text.confirmations",
        "ui_text.nav",
        "ui_text.header",
        "ui_text.templates",
        "ui_text.admin_docs",
        "ui_text.admin_notices",
        "ui_text.empty_states",
        "ui_text.hints",
        "ui_text.counter",
        "ui_text.warnings",
    ]
    missing.extend(_collect_missing(strings_en, required_strings_paths, "strings/en.json"))

    if missing:
        details = "\n".join(missing)
        raise ValueError(f"[karma] Error: missing required configuration keys:\n{details}")

    language = normalize_language((base.get("server") or {}).get("language"))
    if language:
        strings = _load_strings_config(language)
        if not strings:
            available = ", ".join(get_available_languages())
            raise ValueError(
                f"[karma] Error: missing strings/{language}.json. Available languages: {available}"
            )


def set_request_language(language):
    _THREAD_LOCAL.language = normalize_language(language) or None


def get_request_language():
    return getattr(_THREAD_LOCAL, "language", None)


def clear_request_language():
    if hasattr(_THREAD_LOCAL, "language"):
        delattr(_THREAD_LOCAL, "language")


def parse_accept_language(header):
    entries = []
    for part in (header or "").split(","):
        token = part.strip()
        if not token or token == "*":
            continue
        quality = 1.0
        if ";q=" in token:
            lang, q_value = token.split(";q=", 1)
            token = lang.strip()
            try:
                quality = float(q_value)
            except ValueError:
                quality = 0.0
        entries.append((quality, token))
    entries.sort(key=lambda item: item[0], reverse=True)
    return [lang for _, lang in entries]


def match_language(accept_language, available):
    if not accept_language:
        return None
    available_map = {normalize_language(code): code for code in available}
    for lang in parse_accept_language(accept_language):
        normalized = normalize_language(lang)
        if normalized in available_map:
            return available_map[normalized]
        primary = normalized.split("-", 1)[0]
        if primary in available_map:
            return available_map[primary]
    return None


def _require_strings_language(language):
    strings = _load_strings_config(language)
    if not strings:
        raise ValueError(f"[karma] Error: missing strings/{language}.json.")
    return strings


def _build_strings(language):
    default_strings = _require_strings_language("en")
    if language == "en":
        return default_strings
    selected_strings = _load_strings_config(language)
    if not selected_strings:
        raise ValueError(f"[karma] Error: missing strings/{language}.json.")
    return _deep_merge(default_strings, selected_strings)


def get_config(language=None):
    language = normalize_language(language or get_request_language() or get_default_language())
    if not language:
        language = "en"
    if language not in _CONFIG_BY_LANGUAGE:
        base = _BASE_CONFIG or _load_config()
        merged = dict(base)
        community = _COMMUNITY_CONFIG or _load_community_config()
        string_overrides = {}
        if community:
            string_overrides = {k: community[k] for k in ("languages", "ui_text", "placeholders") if k in community}
            merged = _deep_merge(merged, community)
        strings_config = _build_strings(language)
        merged = _deep_merge(merged, strings_config)
        if string_overrides:
            merged = _deep_merge(merged, string_overrides)
        merged.setdefault("server", {})
        merged["server"]["language"] = language
        merged["language"] = language
        _CONFIG_BY_LANGUAGE[language] = merged
    return _CONFIG_BY_LANGUAGE[language]


def require_setting(settings, path, label="config.json"):
    node = settings
    parts = path.split(".")
    for part in parts:
        if not isinstance(node, dict) or part not in node:
            raise ValueError(f"[karma] Error: Missing {path} in {label}. Add it to {label}.")
        node = node[part]
    if node is None or node == "":
        raise ValueError(f"[karma] Error: Missing {path} in {label}. Add it to {label}.")
    return node


def ui_text(path, label="config.json ui_text"):
    return require_setting(get_config(), f"ui_text.{path}", label)


def format_text(template, **values):
    text = str(template)
    for key, value in values.items():
        text = text.replace(f"{{{key}}}", str(value))
    return text


def save_community_config(data):
    global _CONFIG, _COMMUNITY_CONFIG
    community_path = get_community_config_path()
    if not community_path:
        raise ValueError("[karma] Error: community directory not configured.")
    os.makedirs(os.path.dirname(community_path), exist_ok=True)
    with open(community_path, "w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2)
        handle.write("\n")
    _COMMUNITY_CONFIG = data
    _CONFIG = None
    _reset_language_cache()


def get_community_config():
    if _COMMUNITY_CONFIG is None:
        _load_community_config()
    return _COMMUNITY_CONFIG or {}


def get_config_path():
    if _CONFIG_PATH is None:
        _ = get_config()
    return _CONFIG_PATH


def get_config_dir():
    return os.path.dirname(get_config_path())
