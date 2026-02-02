#!/usr/bin/env python3
import os
import signal
import sys


def _require_message(messages, key):
    value = messages.get(key)
    if not isinstance(value, str) or not value:
        raise SystemExit(f"Missing scripts.start.{key} in strings/en.json.")
    return value


def _load_start_messages(settings):
    start_strings = (settings.get("scripts") or {}).get("start", {})
    keys = [
        "usage",
        "port_invalid_integer",
        "stopped",
    ]
    return {key: _require_message(start_strings, key) for key in keys}


def main():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    if root_dir not in sys.path:
        sys.path.insert(0, root_dir)

    from karma import config

    base_language = config.normalize_language(
        (config.get_base_config().get("server") or {}).get("language") or "en"
    )
    base_settings = config.get_config(language=base_language)
    messages = _load_start_messages(base_settings)

    args = sys.argv[1:]
    if not args:
        raise SystemExit(messages["usage"])
    directory = args.pop(0)
    port_override = None
    while args:
        token = args.pop(0)
        if token == "-p":
            if not args:
                raise SystemExit(messages["usage"])
            try:
                port_override = int(args.pop(0))
            except ValueError:
                raise SystemExit(messages["port_invalid_integer"])
        else:
            raise SystemExit(messages["usage"])

    from karma import app
    from karma import cli

    try:
        cli.bootstrap(directory, messages["usage"])
        messages = _load_start_messages(config.get_config())
        if port_override is not None:
            config.get_config().setdefault("server", {})["port"] = port_override
            config.set_port_override(port_override)
    except ValueError as exc:
        print(str(exc))
        raise SystemExit(1)
    def _handle_sigint(_sig, _frame):
        print(f"\n{messages['stopped']}")
        raise SystemExit(0)

    signal.signal(signal.SIGINT, _handle_sigint)
    os.environ["KARMA_SIGINT_HANDLED"] = "1"
    app.main()


if __name__ == "__main__":
    main()
