import base64
import cgi
import hashlib
import hmac
import html
import io
import json
import time
import urllib.parse


class Request:
    def __init__(self, environ):
        self.environ = environ
        self.method = environ.get("REQUEST_METHOD", "GET").upper()
        self.path = environ.get("PATH_INFO", "/")
        self.query_string = environ.get("QUERY_STRING", "")
        self.query = urllib.parse.parse_qs(self.query_string, keep_blank_values=True)
        self._body = None
        self._form = None
        self._multipart = None

    def header(self, name, default=""):
        key = "HTTP_" + name.upper().replace("-", "_")
        return self.environ.get(key, default)

    def body(self):
        if self._body is None:
            try:
                length = int(self.environ.get("CONTENT_LENGTH") or 0)
            except ValueError:
                length = 0
            self._body = self.environ["wsgi.input"].read(length) if length > 0 else b""
        return self._body

    def form(self):
        if self._form is None:
            content_type = self.environ.get("CONTENT_TYPE", "")
            if "application/x-www-form-urlencoded" in content_type:
                body = self.body().decode("utf-8", errors="replace")
                self._form = urllib.parse.parse_qs(body, keep_blank_values=True)
            elif "multipart/form-data" in content_type:
                form, files = self._parse_multipart()
                self._form = form
                self._multipart = (form, files)
            else:
                self._form = {}
        return self._form

    def multipart(self):
        if self._multipart is None:
            content_type = self.environ.get("CONTENT_TYPE", "")
            if "multipart/form-data" in content_type:
                form, files = self._parse_multipart()
            else:
                form, files = {}, {}
            self._multipart = (form, files)
        return self._multipart

    def _parse_multipart(self):
        try:
            length = int(self.environ.get("CONTENT_LENGTH") or 0)
        except ValueError:
            length = 0
        try:
            from karma import config

            settings = config.get_config()
            max_bytes = config.require_setting(settings, "uploads.max_request_bytes")
            if max_bytes is not None and length > int(max_bytes):
                raise ValueError("request_too_large")
        except ValueError:
            raise
        except Exception:
            pass
        form = {}
        files = {}
        body = self.body()
        environ = {
            "REQUEST_METHOD": self.method,
            "CONTENT_TYPE": self.environ.get("CONTENT_TYPE", ""),
            "CONTENT_LENGTH": str(len(body)),
        }
        storage = cgi.FieldStorage(fp=io.BytesIO(body), environ=environ, keep_blank_values=True)
        if storage.list:
            for item in storage.list:
                if item.filename:
                    files[item.name] = item
                else:
                    form.setdefault(item.name, []).append(item.value)
        return form, files


def html_escape(value):
    return html.escape(value or "", quote=True)


def html_response(body, status="200 OK", headers=None):
    headers = headers or []
    headers = [("Content-Type", "text/html; charset=utf-8")] + headers
    try:
        from karma import config

        settings = config.get_config()
        security = settings.get("security_headers", {})
        csp = security.get("content_security_policy")
        referrer = security.get("referrer_policy")
        nosniff = security.get("x_content_type_options")
        existing = {key.lower() for key, _ in headers}
        if csp and "content-security-policy" not in existing:
            headers.append(("Content-Security-Policy", csp))
        if referrer and "referrer-policy" not in existing:
            headers.append(("Referrer-Policy", referrer))
        if nosniff and "x-content-type-options" not in existing:
            headers.append(("X-Content-Type-Options", nosniff))
    except Exception:
        pass
    return status, headers, body.encode("utf-8")


def json_response(payload, status="200 OK", headers=None):
    headers = headers or []
    headers = [("Content-Type", "application/json; charset=utf-8")] + headers
    indent = None
    try:
        from karma import config

        settings = config.get_config()
        if config.require_setting(settings, "debug.pretty_print_json"):
            indent = 2
    except Exception:
        indent = 2
    body = json.dumps(payload, indent=indent, sort_keys=False)
    return status, headers, body.encode("utf-8")


def json_error(error, message=None, status="400 Bad Request", headers=None, extra=None):
    payload = {"ok": False, "error": error}
    if message:
        payload["message"] = message
    if extra:
        payload.update(extra)
    return json_response(payload, status=status, headers=headers)


def redirect(location):
    headers = [("Location", location)]
    return "302 Found", headers, b""


def file_response(body, content_type, status="200 OK", headers=None):
    headers = headers or []
    headers = [("Content-Type", content_type)] + headers
    return status, headers, body


def stream_file_response(path, content_type, headers=None, environ=None, chunk_size=8192):
    headers = headers or []
    headers = [("Content-Type", content_type)] + headers

    def iterator():
        with open(path, "rb") as handle:
            while True:
                chunk = handle.read(chunk_size)
                if not chunk:
                    break
                yield chunk

    return "200 OK", headers, iterator()


def parse_cookies(environ):
    raw = environ.get("HTTP_COOKIE", "")
    cookies = {}
    for part in raw.split(";"):
        if "=" not in part:
            continue
        name, value = part.split("=", 1)
        cookies[name.strip()] = value.strip()
    return cookies


def set_cookie(headers, name, value, path="/", max_age=None, http_only=True, same_site="Lax", secure=False):
    parts = [f"{name}={value}", f"Path={path}", f"SameSite={same_site}"]
    if max_age is not None:
        parts.append(f"Max-Age={max_age}")
    if secure:
        parts.append("Secure")
    if http_only:
        parts.append("HttpOnly")
    headers.append(("Set-Cookie", "; ".join(parts)))


def client_ip(environ):
    remote_addr = environ.get("REMOTE_ADDR", "")
    forwarded = environ.get("HTTP_X_FORWARDED_FOR", "")
    if not forwarded:
        return remote_addr
    try:
        from karma import config

        settings = config.get_config()
        trusted = (settings.get("server") or {}).get("trusted_proxies", [])
        if isinstance(trusted, (list, tuple)) and remote_addr in trusted:
            for part in forwarded.split(","):
                candidate = part.strip()
                if candidate:
                    return candidate
    except Exception:
        pass
    return remote_addr


def sign_session(payload, secret, expires_in=3600):
    expiry = int(time.time()) + expires_in
    message = f"{payload}|{expiry}"
    signature = hmac.new(secret.encode("utf-8"), message.encode("utf-8"), hashlib.sha256).hexdigest()
    token = f"{message}|{signature}"
    return base64.urlsafe_b64encode(token.encode("utf-8")).decode("utf-8")


def verify_session(token, secret):
    try:
        decoded = base64.urlsafe_b64decode(token.encode("utf-8")).decode("utf-8")
        payload, expiry, signature = decoded.rsplit("|", 2)
        message = f"{payload}|{expiry}"
        expected = hmac.new(secret.encode("utf-8"), message.encode("utf-8"), hashlib.sha256).hexdigest()
        if not hmac.compare_digest(signature, expected):
            return None
        if int(expiry) < int(time.time()):
            return None
        return payload
    except Exception:
        return None
