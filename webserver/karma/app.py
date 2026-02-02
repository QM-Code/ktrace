import os
import time
import logging

try:
    from waitress import serve as serve_app
except ImportError:
    serve_app = None
from wsgiref.simple_server import make_server

from karma import auth, config, db, router, views, webhttp
from karma import logging_utils


def application(environ, start_response):
    start_time = time.time()
    request = webhttp.Request(environ)
    pending_headers = []
    if request.method == "GET":
        token = auth.ensure_csrf_cookie(request, pending_headers)
        if token:
            request.environ["KARMA_CSRF_TOKEN"] = token
    status = "500 Internal Server Error"
    headers = [("Content-Type", "text/plain; charset=utf-8")]
    body = b"Internal Server Error"
    try:
        result = router.dispatch(request)
        if result is None:
            result = views.error_page("404 Not Found", "not_found")
        status, headers, body = result
    except Exception:
        logging.getLogger("karma").exception(
            "Unhandled exception for %s %s",
            request.method,
            request.path,
        )
    if pending_headers:
        content_type = ""
        for key, value in headers:
            if key.lower() == "content-type":
                content_type = value or ""
                break
        if content_type.startswith("text/html"):
            headers = headers + pending_headers
    start_response(status, headers)
    elapsed_ms = (time.time() - start_time) * 1000.0
    try:
        status_code = int(status.split()[0])
    except (ValueError, IndexError):
        status_code = 0
    if isinstance(body, (bytes, bytearray)):
        body_len = len(body)
    else:
        body_len = 0
    message = '%s "%s %s" %s %s %.2fms' % (
        webhttp.client_ip(environ),
        request.method,
        request.path,
        status_code,
        body_len,
        elapsed_ms,
    )
    if logging_utils.include_user_enabled():
        user_label = "-"
        try:
            user = auth.get_user_from_request(request)
        except Exception:
            user = None
        if user:
            user_label = auth.display_username(user)
        message = f"{message} user={user_label}"
    logging_utils.log_access(request.path, message)
    if isinstance(body, (bytes, bytearray)):
        return [body]
    return body


def main():
    try:
        settings = config.get_config()
        config.validate_startup(settings)
        if not config.get_community_dir():
            raise ValueError("[karma] Error: community directory must be provided.")
        logging_utils.init_logging(settings, config.get_community_dir())
        community_settings = config.get_community_config()
        session_secret = (community_settings.get("server") or {}).get("session_secret", "")
        if not session_secret or session_secret == "CHANGE_ME":
            raise ValueError("[karma] Error: community config.json must define a unique session_secret.")
        db.init_db(db.default_db_path())
    except ValueError as exc:
        print(str(exc))
        return
    host = config.require_setting(settings, "server.host")
    port = (settings.get("server") or {}).get("port")
    if port is None:
        print("[karma] Error: No port specified in config.json. Use -p <port> to specify a port.")
        return
    port = int(port)
    print(f"Community server listening on http://{host}:{port}")
    try:
        if serve_app is not None:
            threads = int(config.require_setting(settings, "httpserver.threads"))
            serve_app(application, host=host, port=port, threads=threads)
            return
        with make_server(host, port, application) as server:
            server.serve_forever()
    except OSError as exc:
        if getattr(exc, "errno", None) in (98, 10048) or "Address already in use" in str(exc):
            community_dir = config.get_community_dir() or "<community-dir>"
            community_dir = community_dir.rstrip("/\\")
            if config.get_port_override() is not None:
                print(f"[karma] Error: Port {port} is already in use.")
                return
            print(
                f"[karma] Error: Port {port} is already in use.\n"
                f"[karma] Change the port in {community_dir}/config.json or use the -p <port> flag."
            )
            return
        raise
    except KeyboardInterrupt:
        if os.environ.get("KARMA_SIGINT_HANDLED") != "1":
            print("\nServer stopped.")


if __name__ == "__main__":
    main()
