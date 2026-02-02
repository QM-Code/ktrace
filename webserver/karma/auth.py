import hashlib
import hmac
import secrets

from karma import config, db, webhttp

_CSRF_COOKIE = "csrf_token"
_CSRF_COOKIE_AGE = 2 * 3600


def hash_password(password, salt):
    return hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt.encode("utf-8"), 100_000).hex()


def new_password(password):
    salt = secrets.token_hex(16)
    digest = hash_password(password, salt)
    return digest, salt


def verify_password(password, salt, digest):
    supplied = hash_password(password, salt)
    return hmac.compare_digest(supplied, digest)


def csrf_token(request):
    cookies = webhttp.parse_cookies(request.environ)
    session = cookies.get("user_session", "")
    if not session:
        return request.environ.get("KARMA_CSRF_TOKEN", "") or cookies.get(_CSRF_COOKIE, "")
    secret = config.require_setting(config.get_config(), "server.session_secret")
    payload = webhttp.verify_session(session, secret)
    if not payload:
        return request.environ.get("KARMA_CSRF_TOKEN", "") or cookies.get(_CSRF_COOKIE, "")
    return hmac.new(secret.encode("utf-8"), session.encode("utf-8"), hashlib.sha256).hexdigest()


def verify_csrf(request, form):
    expected = csrf_token(request)
    if not expected:
        return False
    supplied = form.get("csrf_token", "")
    if isinstance(supplied, list):
        supplied = supplied[0] if supplied else ""
    if not supplied:
        return False
    return hmac.compare_digest(supplied, expected)


def ensure_csrf_cookie(request, headers):
    cookies = webhttp.parse_cookies(request.environ)
    existing = cookies.get(_CSRF_COOKIE, "")
    if existing:
        return existing
    token = secrets.token_hex(16)
    cookie = cookie_settings()
    webhttp.set_cookie(
        headers,
        _CSRF_COOKIE,
        token,
        max_age=_CSRF_COOKIE_AGE,
        http_only=cookie["http_only"],
        same_site=cookie["same_site"],
        secure=cookie["secure"],
    )
    return token


def sign_user_session(user_id):
    secret = config.require_setting(config.get_config(), "server.session_secret")
    return webhttp.sign_session(str(user_id), secret, expires_in=8 * 3600)


def sign_admin_session(username):
    secret = config.require_setting(config.get_config(), "server.session_secret")
    return webhttp.sign_session(f"admin:{username}", secret, expires_in=8 * 3600)


def cookie_settings(settings=None):
    settings = settings or config.get_config()
    cookie = config.require_setting(settings, "session_cookie")
    secure = bool(config.require_setting(cookie, "secure", "config.json session_cookie"))
    same_site = str(config.require_setting(cookie, "same_site", "config.json session_cookie"))
    http_only = bool(config.require_setting(cookie, "http_only", "config.json session_cookie"))
    return {"secure": secure, "same_site": same_site, "http_only": http_only}


def get_user_from_request(request):
    cookies = webhttp.parse_cookies(request.environ)
    token = cookies.get("user_session", "")
    if not token:
        return None
    payload = webhttp.verify_session(token, config.require_setting(config.get_config(), "server.session_secret"))
    if not payload or not payload.isdigit():
        if payload and payload.startswith("admin:"):
            username = payload.split("admin:", 1)[1] or config.require_setting(config.get_config(), "server.admin_user")
            return {
                "id": None,
                "username": username,
                "email": "",
                "is_admin": 1,
            }
        return None
    with db.connect_ctx() as conn:
        user = db.get_user_by_id(conn, int(payload))
        if not user:
            return None
        if user["is_locked"] or user["deleted"]:
            return None
        return user


def ensure_admin_user(settings, conn):
    admin_user = config.require_setting(settings, "server.admin_user")
    admin_row = db.get_user_by_username(conn, admin_user)
    if admin_row:
        return dict(admin_row)
    return None


def is_admin(user):
    return bool(user and user["is_admin"])


def display_username(user):
    if not user:
        return None
    admin_user = config.require_setting(config.get_config(), "server.admin_user")
    name = None
    if isinstance(user, dict):
        name = user.get("username")
    else:
        try:
            name = user["username"]
        except Exception:
            name = None
    if not name:
        return None
    if str(name).lower() == str(admin_user).lower():
        return admin_user
    return name


def user_token(user):
    if not user:
        return None
    code = None
    try:
        code = user.get("code")
    except AttributeError:
        try:
            code = user["code"] if "code" in user.keys() else None
        except Exception:
            code = None
    if code:
        return str(code)
    name = None
    try:
        name = user.get("username")
    except AttributeError:
        try:
            name = user["username"]
        except Exception:
            name = None
    return str(name) if name else None
