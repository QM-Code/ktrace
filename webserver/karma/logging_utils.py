import logging
import os
from logging.handlers import RotatingFileHandler, TimedRotatingFileHandler

from karma import config


_INITIALIZED = False
_ACCESS_RULES = []
_ACCESS_LOGGERS = {}
_INCLUDE_USER = False


def _path_matches(rule_path, request_path):
    if rule_path == "/":
        return True
    if request_path == rule_path:
        return True
    if rule_path.endswith("/"):
        return request_path.startswith(rule_path)
    return request_path.startswith(rule_path + "/")


def _handler_from_settings(settings, log_path):
    rotation = str(config.require_setting(settings, "logging.rotation", "config.json logging.rotation")).strip().lower()
    backups = int(config.require_setting(settings, "logging.backups", "config.json logging.backups"))
    if rotation == "daily":
        use_utc = bool(config.require_setting(settings, "logging.utc", "config.json logging.utc"))
        return TimedRotatingFileHandler(
            log_path,
            when="midnight",
            backupCount=backups,
            utc=use_utc,
            encoding="utf-8",
        )
    if rotation == "size":
        max_bytes = int(config.require_setting(settings, "logging.max_bytes", "config.json logging.max_bytes"))
        return RotatingFileHandler(
            log_path,
            maxBytes=max_bytes,
            backupCount=backups,
            encoding="utf-8",
        )
    raise ValueError("[karma] Error: logging.rotation must be 'daily' or 'size'.")


def _get_access_logger(settings, log_path, level):
    logger = _ACCESS_LOGGERS.get(log_path)
    if logger:
        return logger
    handler = _handler_from_settings(settings, log_path)
    handler.setFormatter(logging.Formatter("%(asctime)s %(message)s"))
    logger = logging.getLogger(f"karma.access.{log_path}")
    logger.setLevel(level)
    logger.propagate = False
    logger.addHandler(handler)
    _ACCESS_LOGGERS[log_path] = logger
    return logger


def init_logging(settings, community_dir):
    global _INITIALIZED, _INCLUDE_USER
    if _INITIALIZED:
        return
    enabled = bool(config.require_setting(settings, "logging.enabled", "config.json logging.enabled"))
    if not enabled:
        _INITIALIZED = True
        return

    log_dir_name = config.require_setting(settings, "logging.dir", "config.json logging.dir")
    log_dir = os.path.join(community_dir, str(log_dir_name))
    os.makedirs(log_dir, exist_ok=True)

    level_name = str(config.require_setting(settings, "logging.level", "config.json logging.level")).upper()
    level = getattr(logging, level_name, logging.INFO)
    _INCLUDE_USER = bool(config.require_setting(settings, "logging.include_user", "config.json logging.include_user"))

    server_file = config.require_setting(settings, "logging.server_file", "config.json logging.server_file")
    server_path = os.path.join(log_dir, str(server_file))
    server_handler = _handler_from_settings(settings, server_path)

    server_handler.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(name)s %(message)s"))

    server_logger = logging.getLogger("karma")
    server_logger.setLevel(level)
    server_logger.propagate = False
    server_logger.addHandler(server_handler)

    for name in ("waitress", "waitress.access", "waitress.error"):
        waitress_logger = logging.getLogger(name)
        waitress_logger.setLevel(level)
        waitress_logger.propagate = False
        waitress_logger.addHandler(server_handler)

    paths = config.require_setting(settings, "logging.paths", "config.json logging.paths")
    if not isinstance(paths, (list, dict)):
        raise ValueError("[karma] Error: logging.paths must be a list or map.")
    rules = []
    if isinstance(paths, list):
        entries = []
        for index, entry in enumerate(paths):
            if not isinstance(entry, dict):
                raise ValueError("[karma] Error: logging.paths entries must be objects.")
            path_value = entry.get("path")
            entries.append((index, path_value, entry))
    else:
        entries = []
        for index, (path_key, entry) in enumerate(paths.items()):
            entries.append((index, path_key, entry))
    for index, path_value, entry in entries:
        if not isinstance(entry, dict):
            raise ValueError("[karma] Error: logging.paths entries must be objects.")
        if not isinstance(path_value, str) or not path_value.startswith("/"):
            raise ValueError("[karma] Error: logging.paths entry path must be a string starting with '/'.")
        file_value = entry.get("file")
        if file_value is not None:
            if not isinstance(file_value, str) or not file_value:
                raise ValueError("[karma] Error: logging.paths entry file must be a non-empty string or null.")
        override = bool(entry.get("override", False))
        rules.append(
            {
                "path": path_value,
                "file": file_value,
                "override": override,
                "order": index,
            }
        )
    rules.sort(key=lambda rule: (-len(rule["path"]), rule["order"]))
    for rule in rules:
        if rule["file"] is not None:
            rule["logger"] = _get_access_logger(settings, os.path.join(log_dir, rule["file"]), level)
        else:
            rule["logger"] = None
    _ACCESS_RULES[:] = rules

    _INITIALIZED = True


def include_user_enabled():
    return _INCLUDE_USER


def log_access(request_path, message):
    if not _ACCESS_RULES:
        return
    matched = False
    for rule in _ACCESS_RULES:
        if not _path_matches(rule["path"], request_path):
            continue
        matched = True
        logger = rule.get("logger")
        if logger:
            logger.info(message)
        if rule["override"]:
            break
    if not matched:
        return
