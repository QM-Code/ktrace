import secrets
import time
from urllib.parse import quote

from karma import auth, config, db, ratelimit, views, webhttp


def _first(form, key):
    value = form.get(key, [""])
    return value[0].strip()


def _form_values(form, keys):
    return {key: _first(form, key) for key in keys}


def _msg(key, **values):
    template = config.ui_text(f"messages.account.{key}", "config.json ui_text.messages.account")
    return config.format_text(template, **values)


def _no_store_headers():
    return [("Cache-Control", "no-store"), ("Pragma", "no-cache")]


def _label(key):
    return config.ui_text(f"labels.{key}", "config.json ui_text.labels")


def _action(key):
    return config.ui_text(f"actions.{key}", "config.json ui_text.actions")


def _title(key):
    return config.ui_text(f"titles.{key}", "config.json ui_text.titles")


def _render_register(request, message=None, form_data=None, headers=None):
    form_data = form_data or {}
    username_value = webhttp.html_escape(form_data.get("username", ""))
    email_value = webhttp.html_escape(form_data.get("email", ""))
    csrf_html = views.csrf_input(auth.csrf_token(request))
    body = f"""<form method="post" action="/register">
  {csrf_html}
  <div class="row">
    <div>
      <label for="username">{webhttp.html_escape(_label("username"))}</label>
      <input id="username" name="username" required value="{username_value}">
    </div>
    <div>
      <label for="email">{webhttp.html_escape(_label("email"))}</label>
      <input id="email" name="email" type="email" required value="{email_value}">
    </div>
  </div>
  <div class="row">
    <div>
      <label for="password">{webhttp.html_escape(_label("password"))}</label>
      <input id="password" name="password" type="password" required>
    </div>
  </div>
  <div class="actions">
    <button type="submit">{webhttp.html_escape(_action("create_account"))}</button>
    <a class="admin-link align-right" href="/login">{webhttp.html_escape(_action("already_have_account"))}</a>
  </div>
</form>
"""
    header_html = views.header_with_title(
        config.require_setting(config.get_config(), "server.community_name"),
        "/register",
        logged_in=False,
        title=_title("create_account"),
        error=message,
        is_admin=False,
    )
    return views.render_page_with_header(_title("create_account"), header_html, body, headers=headers)


def _render_login(request, message=None, list_name=None, logged_in=False, form_data=None, headers=None):
    form_data = form_data or {}
    identifier_value = webhttp.html_escape(form_data.get("identifier", ""))
    csrf_html = views.csrf_input(auth.csrf_token(request))
    body = f"""<form method="post" action="/login">
  {csrf_html}
  <div class="row">
    <div>
      <label for="email">{webhttp.html_escape(_label("email_or_username"))}</label>
      <input id="email" name="email" required value="{identifier_value}" autofocus>
    </div>
    <div>
      <label for="password">{webhttp.html_escape(_label("password"))}</label>
      <input id="password" name="password" type="password" required>
    </div>
  </div>
  <div class="actions">
    <button type="submit">{webhttp.html_escape(_action("sign_in"))}</button>
    <a class="admin-link align-right" href="/forgot">{webhttp.html_escape(_action("forgot_password"))}</a>
  </div>
</form>
<p class="muted">{webhttp.html_escape(_action("need_account"))} <a class="admin-link" href="/register">{webhttp.html_escape(_action("register_here"))}</a></p>
"""
    header_html = ""
    if list_name is not None:
        header_html = views.header_with_title(
            list_name,
            "/login",
            logged_in,
            title=_title("login"),
            show_login=False,
            error=message,
            is_admin=False,
        )
    return views.render_page_with_header(_title("login"), header_html, body, headers=headers)


def _render_forgot(request, message=None, reset_link=None, level="info", form_data=None, headers=None):
    link_html = ""
    if reset_link:
        prefix = webhttp.html_escape(config.ui_text("messages.account.reset_link_prefix", "config.json ui_text.messages.account"))
        link_html = f'<p class="muted">{prefix} <a href="{reset_link}">{reset_link}</a></p>'
    form_data = form_data or {}
    email_value = webhttp.html_escape(form_data.get("email", ""))
    csrf_html = views.csrf_input(auth.csrf_token(request))
    body = f"""<form method="post" action="/forgot">
  {csrf_html}
  <div>
    <label for="email">{webhttp.html_escape(_label("email"))}</label>
    <input id="email" name="email" type="email" required value="{email_value}">
  </div>
  <div class="actions">
    <button type="submit">{webhttp.html_escape(_action("generate_reset_link"))}</button>
    <a class="admin-link align-right" href="/login">{webhttp.html_escape(_action("back_to_login"))}</a>
  </div>
</form>
{link_html}
<p class="muted">{webhttp.html_escape(config.ui_text("messages.account.reset_links_notice", "config.json ui_text.messages.account"))}</p>
"""
    notice_kwargs = {"info": None, "warning": None, "error": None}
    if level in notice_kwargs:
        notice_kwargs[level] = message
        header_html = views.header_with_title(
        config.require_setting(config.get_config(), "server.community_name"),
        "/forgot",
        logged_in=False,
        title=_title("reset_password"),
        **notice_kwargs,
        is_admin=False,
    )
    return views.render_page_with_header(_title("reset_password"), header_html, body, headers=headers)


def _render_reset(request, token, message=None, headers=None):
    csrf_html = views.csrf_input(auth.csrf_token(request))
    body = f"""<form method="post" action="/reset">
  {csrf_html}
  <input type="hidden" name="token" value="{webhttp.html_escape(token)}">
  <div class="row">
    <div>
      <label for="password">{webhttp.html_escape(_label("new_password"))}</label>
      <input id="password" name="password" type="password" required>
    </div>
  </div>
  <div class="actions">
    <button type="submit">{webhttp.html_escape(_action("update_password"))}</button>
    <a class="admin-link align-right" href="/login">{webhttp.html_escape(_action("back_to_login"))}</a>
  </div>
</form>
"""
    header_html = views.header_with_title(
        config.require_setting(config.get_config(), "server.community_name"),
        "/reset",
        logged_in=False,
        title=_title("set_new_password"),
        error=message,
        is_admin=False,
    )
    return views.render_page_with_header(_title("set_new_password"), header_html, body, headers=headers)


def _normalize_key(value):
    return value.strip().lower()


def _owns_server(user, server):
    return server["owner_user_id"] == user["id"]


def _is_root_admin(user, settings):
    admin_username = config.require_setting(settings, "server.admin_user")
    return _normalize_key(user["username"]) == _normalize_key(admin_username)


def _sync_root_admin_privileges(conn, settings):
    admin_username = config.require_setting(settings, "server.admin_user")
    root_user = db.get_user_by_username(conn, admin_username)
    if not root_user:
        return
    db.recompute_admin_flags(conn, root_user["id"])


def _recompute_root_admins(conn, user, settings):
    if not _is_root_admin(user, settings):
        admin_username = config.require_setting(settings, "server.admin_user")
        root_user = db.get_user_by_username(conn, admin_username)
        if not root_user:
            return
        admins = db.list_user_admins(conn, root_user["id"])
        trusted_admin_ids = {
            admin["admin_user_id"] for admin in admins if admin["trust_admins"]
        }
        if user["id"] not in trusted_admin_ids:
            return
    else:
        admin_username = config.require_setting(settings, "server.admin_user")
        root_user = db.get_user_by_username(conn, admin_username)
        if not root_user:
            return
    if root_user:
        db.recompute_admin_flags(conn, root_user["id"])


def _trusted_primary_notice(conn, target_user, settings):
    admin_username = config.require_setting(settings, "server.admin_user")
    root_user = db.get_user_by_username(conn, admin_username)
    if not root_user:
        return ""
    admins = db.list_user_admins(conn, root_user["id"])
    trusted_ids = {
        admin["admin_user_id"] for admin in admins if admin["trust_admins"]
    }
    if target_user["id"] == root_user["id"]:
        return config.ui_text("admin_notices.root_html")
    if target_user["id"] not in trusted_ids:
        return ""
    return config.ui_text("admin_notices.trusted_html")


def handle(request):
    path = request.path.rstrip("/")
    settings = config.get_config()
    with db.connect_ctx() as conn:
        if path == "/register":
            if request.method == "GET":
                status, response_headers, body = _render_register(
                    request,
                    headers=_no_store_headers(),
                )
                return status, response_headers, body
            if request.method == "POST":
                form = request.form()
                if not auth.verify_csrf(request, form):
                    return views.error_page("403 Forbidden", "forbidden")
                if not ratelimit.check(settings, request, "register"):
                    return _render_register(
                        request,
                        _msg("rate_limited"),
                        headers=_no_store_headers(),
                    )
                username = _first(form, "username")
                email = _first(form, "email").lower()
                password = _first(form, "password")
                form_data = _form_values(form, ["username", "email"])
                if not username or not email or not password:
                    return _render_register(
                        request,
                        _msg("register_missing_fields"),
                        form_data=form_data,
                        headers=_no_store_headers(),
                    )
                if " " in username:
                    return _render_register(
                        request,
                        _msg("register_username_spaces"),
                        form_data=form_data,
                        headers=_no_store_headers(),
                    )
                admin_username = config.require_setting(settings, "server.admin_user")
                if _normalize_key(username) == _normalize_key(admin_username):
                    return _render_register(
                        request,
                        _msg("register_username_reserved"),
                        form_data=form_data,
                        headers=_no_store_headers(),
                    )
                if db.get_user_by_username(conn, username):
                    return _render_register(
                        request,
                        _msg("register_username_taken", username=username),
                        form_data=form_data,
                        headers=_no_store_headers(),
                    )
                if db.get_user_by_email(conn, email):
                    return _render_register(
                        request,
                        _msg("register_email_taken"),
                        form_data=form_data,
                        headers=_no_store_headers(),
                    )
                digest, salt = auth.new_password(password)
                db.add_user(conn, username, email, digest, salt)
                user = db.get_user_by_email(conn, email)
                headers = []
                token = auth.sign_user_session(user["id"])
                cookie = auth.cookie_settings(settings)
                webhttp.set_cookie(
                    headers,
                    "user_session",
                    token,
                    max_age=8 * 3600,
                    http_only=cookie["http_only"],
                    same_site=cookie["same_site"],
                    secure=cookie["secure"],
                )
                profile_url = "/servers"
                if user and "username" in user:
                    user_token = auth.user_token(user)
                    profile_url = f"/users/{quote(user_token, safe='')}"
                status, redirect_headers, body = webhttp.redirect(profile_url)
                return status, headers + redirect_headers, body
            return views.error_page("405 Method Not Allowed", "method_not_allowed")

        if path == "/login":
            if request.method == "GET":
                list_name = config.require_setting(settings, "server.community_name")
                status, response_headers, body = _render_login(
                    request,
                    list_name=list_name,
                    headers=_no_store_headers(),
                )
                return status, response_headers, body
            if request.method == "POST":
                form = request.form()
                if not auth.verify_csrf(request, form):
                    return views.error_page("403 Forbidden", "forbidden")
                if not ratelimit.check(settings, request, "login"):
                    return _render_login(
                        request,
                        _msg("rate_limited"),
                        list_name=config.require_setting(settings, "server.community_name"),
                        headers=_no_store_headers(),
                    )
                identifier = _first(form, "email")
                password = _first(form, "password")
                form_data = {"identifier": identifier}
                if "@" in identifier:
                    user = db.get_user_by_email(conn, identifier.lower())
                else:
                    user = db.get_user_by_username(conn, identifier)
                if not user or not auth.verify_password(password, user["password_salt"], user["password_hash"]):
                    return _render_login(
                        request,
                        _msg("login_failed"),
                        list_name=config.require_setting(settings, "server.community_name"),
                        form_data=form_data,
                        headers=_no_store_headers(),
                    )
                if user and (user["is_locked"] or user["deleted"]):
                    return _render_login(
                        request,
                        _msg("account_locked"),
                        list_name=config.require_setting(settings, "server.community_name"),
                        form_data=form_data,
                        headers=_no_store_headers(),
                    )
                headers = []
                if isinstance(user, dict) and user.get("id") is None:
                    token = auth.sign_admin_session(user["username"])
                else:
                    token = auth.sign_user_session(user["id"])
                cookie = auth.cookie_settings(settings)
                webhttp.set_cookie(
                    headers,
                    "user_session",
                    token,
                    max_age=8 * 3600,
                    http_only=cookie["http_only"],
                    same_site=cookie["same_site"],
                    secure=cookie["secure"],
                )
                profile_url = "/servers"
                if user and "username" in user:
                    user_token = auth.user_token(user)
                    profile_url = f"/users/{quote(user_token, safe='')}"
                status, redirect_headers, body = webhttp.redirect(profile_url)
                return status, headers + redirect_headers, body
            return views.error_page("405 Method Not Allowed", "method_not_allowed")

        if path == "/logout":
            if request.method not in ("GET", "POST"):
                return views.error_page("405 Method Not Allowed", "method_not_allowed")
            headers = []
            cookie = auth.cookie_settings(settings)
            webhttp.set_cookie(
                headers,
                "user_session",
                "expired",
                max_age=0,
                http_only=cookie["http_only"],
                same_site=cookie["same_site"],
                secure=cookie["secure"],
            )
            status, redirect_headers, body = webhttp.redirect("/servers")
            return status, headers + redirect_headers, body

        if path == "/forgot":
            if request.method == "GET":
                status, response_headers, body = _render_forgot(
                    request,
                    headers=_no_store_headers(),
                )
                return status, response_headers, body
            if request.method == "POST":
                form = request.form()
                if not auth.verify_csrf(request, form):
                    return views.error_page("403 Forbidden", "forbidden")
                if not ratelimit.check(settings, request, "forgot"):
                    return _render_forgot(
                        request,
                        _msg("rate_limited"),
                        level="warning",
                        headers=_no_store_headers(),
                    )
                email = _first(form, "email").lower()
                form_data = _form_values(form, ["email"])
                db.delete_expired_password_resets(conn, int(time.time()))
                user = db.get_user_by_email(conn, email)
                reset_link = None
                if user:
                    token = secrets.token_hex(24)
                    expires_at = int(time.time()) + 3600
                    db.add_password_reset(conn, user["id"], token, expires_at)
                    reset_link = f"/reset?token={token}"
                return _render_forgot(
                    request,
                    _msg("forgot_notice"),
                    reset_link=reset_link,
                    level="info",
                    form_data=form_data,
                    headers=_no_store_headers(),
                )
            return views.error_page("405 Method Not Allowed", "method_not_allowed")

        if path == "/reset":
            if request.method == "GET":
                token = request.query.get("token", [""])[0]
                db.delete_expired_password_resets(conn, int(time.time()))
                reset = db.get_password_reset(conn, token)
                if not reset or reset["expires_at"] < int(time.time()):
                    status, response_headers, body = _render_forgot(
                        request,
                        _msg("reset_invalid"),
                        level="error",
                        headers=_no_store_headers(),
                    )
                    return status, response_headers, body
                status, response_headers, body = _render_reset(
                    request,
                    token,
                    headers=_no_store_headers(),
                )
                return status, response_headers, body
            if request.method == "POST":
                form = request.form()
                if not auth.verify_csrf(request, form):
                    return views.error_page("403 Forbidden", "forbidden")
                if not ratelimit.check(settings, request, "reset"):
                    return _render_forgot(
                        request,
                        _msg("rate_limited"),
                        level="warning",
                        headers=_no_store_headers(),
                    )
                token = _first(form, "token")
                password = _first(form, "password")
                db.delete_expired_password_resets(conn, int(time.time()))
                reset = db.get_password_reset(conn, token)
                if not reset or reset["expires_at"] < int(time.time()):
                    return _render_forgot(
                        request,
                        _msg("reset_invalid"),
                        level="error",
                        headers=_no_store_headers(),
                    )
                if not password:
                    return _render_reset(
                        request,
                        token,
                        _msg("reset_password_required"),
                        headers=_no_store_headers(),
                    )
                digest, salt = auth.new_password(password)
                db.set_user_password(conn, reset["user_id"], digest, salt)
                db.delete_password_reset(conn, token)
                return _render_login(
                    request,
                    _msg("reset_success"),
                    list_name=config.require_setting(settings, "server.community_name"),
                    headers=_no_store_headers(),
                )
            return views.error_page("405 Method Not Allowed", "method_not_allowed")

    return views.error_page("404 Not Found", "not_found")
