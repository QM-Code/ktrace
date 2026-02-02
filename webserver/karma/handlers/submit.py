import time
from urllib.parse import quote

from karma import auth, config, db, uploads, views, webhttp


def _first(form, key):
    value = form.get(key, [""])
    return value[0].strip()


def _parse_int(value):
    if value == "":
        return None
    try:
        return int(value)
    except ValueError:
        return None


def _format_template(template, **values):
    result = str(template)
    for key, value in values.items():
        result = result.replace(f"{{{key}}}", str(value))
    return result


def _msg(key, **values):
    template = config.ui_text(f"messages.submit.{key}", "config.json ui_text.messages.submit")
    return config.format_text(template, **values)


def _label(key):
    return config.ui_text(f"labels.{key}", "config.json ui_text.labels")


def _action(key):
    return config.ui_text(f"actions.{key}", "config.json ui_text.actions")


def _title(key):
    return config.ui_text(f"titles.{key}", "config.json ui_text.titles")


def _render_form(request, message=None, user=None, form_data=None, owner_token=""):
    form_data = form_data or {}
    settings = config.get_config()
    ui_text = config.require_setting(settings, "ui_text")
    overview_max = int(config.require_setting(settings, "pages.servers.overview_max_chars"))
    counter_template = config.require_setting(ui_text, "counter.overview", "config.json ui_text.counter")
    warning_template = config.require_setting(ui_text, "warnings.overview_over_limit", "config.json ui_text.warnings")
    markdown_hint = config.require_setting(ui_text, "hints.markdown_supported", "config.json ui_text.hints")
    placeholders = settings.get("placeholders", {}).get("add_server", {})
    host_value = webhttp.html_escape(form_data.get("host", ""))
    port_value = webhttp.html_escape(form_data.get("port", ""))
    name_value = webhttp.html_escape(form_data.get("name", ""))
    overview_value = webhttp.html_escape(form_data.get("overview", ""))
    description_value = webhttp.html_escape(form_data.get("description", ""))
    def _ph(key):
        value = placeholders.get(key, "")
        if not value:
            return ""
        return f' placeholder="{webhttp.html_escape(value)}"'
    csrf_html = views.csrf_input(auth.csrf_token(request))
    owner_html = ""
    if owner_token:
        owner_html = f'<input type="hidden" name="owner" value="{webhttp.html_escape(owner_token)}">'
    body = f"""<form method="post" action="/servers/add" enctype="multipart/form-data">
  {csrf_html}
  {owner_html}
  <div class="row">
    <div>
      <label for="host">{webhttp.html_escape(_label("host"))}</label>
      <input id="host" name="host" required{_ph("host")} value="{host_value}">
    </div>
    <div>
      <label for="port">{webhttp.html_escape(_label("port"))}</label>
      <input id="port" name="port" required{_ph("port")} value="{port_value}">
    </div>
  </div>
  <div>
    <label for="name">{webhttp.html_escape(_label("server_name"))}</label>
    <input id="name" name="name" required{_ph("name")} value="{name_value}">
  </div>
  <div>
    <label for="overview">{webhttp.html_escape(_label("overview"))}</label>
    <div class="field-group" data-char-field>
      <textarea id="overview" name="overview"{_ph("overview")} data-char-limit="{overview_max}" data-counter-template="{webhttp.html_escape(counter_template)}" data-warning-template="{webhttp.html_escape(warning_template)}">{overview_value}</textarea>
      <div class="field-meta">
        <span class="char-counter"></span>
        <span class="char-warning hidden"></span>
      </div>
    </div>
  </div>
  <div>
    <label for="description">{webhttp.html_escape(_label("description"))}</label>
    <textarea id="description" name="description"{_ph("description")}>{description_value}</textarea>
    <p class="muted hint">{webhttp.html_escape(markdown_hint)}</p>
  </div>
"""
    body += f"""
  <div>
    <label for="screenshot">{webhttp.html_escape(_label("screenshot_upload"))}</label>
    <input id="screenshot" name="screenshot" type="file" accept="image/*">
  </div>
  <div class="actions">
    <button type="submit">{webhttp.html_escape(_action("submit_for_approval"))}</button>
    <a class="admin-link align-right" href="/servers">{webhttp.html_escape(_action("cancel"))}</a>
  </div>
</form>
"""
    profile_url = None
    if user:
        user_token = auth.user_token(user)
        profile_url = f"/users/{quote(user_token, safe='')}"
    header_html = views.header_with_title(
        config.require_setting(config.get_config(), "server.community_name"),
        "/servers/add",
        logged_in=bool(user),
        title=_title("submit_server"),
        error=message,
        user_name=auth.display_username(user),
        is_admin=auth.is_admin(user),
        profile_url=profile_url,
    )
    return views.render_page_with_header(_title("submit_server"), header_html, body)


def _render_success(user=None, owner_token=""):
    body = f"""<p class="muted">{webhttp.html_escape(_msg("success_notice"))}</p>
<div class="actions">
  <a class="admin-link" href="/servers">{webhttp.html_escape(_action("return_to_servers"))}</a>
  <a class="admin-link" href="/servers/add">{webhttp.html_escape(_action("submit_another"))}</a>
</div>
"""
    profile_url = None
    if user:
        user_token = auth.user_token(user)
        profile_url = f"/users/{quote(user_token, safe='')}"
    header_html = views.header_with_title(
        config.require_setting(config.get_config(), "server.community_name"),
        "/servers/add",
        logged_in=True,
        title=_title("server_added"),
        user_name=auth.display_username(user),
        is_admin=auth.is_admin(user),
        profile_url=profile_url,
    )
    if owner_token:
        return webhttp.redirect(f"/users/{quote(owner_token, safe='')}")
    return views.render_page_with_header(_title("server_added"), header_html, body)

def handle(request):
    if request.method == "GET":
        user = auth.get_user_from_request(request)
        if not user:
            return webhttp.redirect("/login")
        owner_token = request.query.get("owner", [""])[0].strip()
        if owner_token and auth.is_admin(user):
            with db.connect_ctx() as conn:
                owner_user = db.get_user_by_code(conn, owner_token)
                if not owner_user:
                    owner_user = db.get_user_by_username(conn, owner_token)
                if not owner_user or owner_user["deleted"]:
                    message = config.ui_text("messages.users.user_not_found")
                    return _render_form(request, message, user=user)
        else:
            owner_token = ""
        return _render_form(request, user=user, owner_token=owner_token)
    if request.method != "POST":
        return views.error_page("405 Method Not Allowed", "method_not_allowed")

    settings = config.get_config()
    user = auth.get_user_from_request(request)
    if not user:
        return webhttp.redirect("/login")
    overview_max = int(config.require_setting(settings, "pages.servers.overview_max_chars"))
    max_bytes = int(config.require_setting(settings, "uploads.max_request_bytes"))
    try:
        content_length = int(request.environ.get("CONTENT_LENGTH") or 0)
    except ValueError:
        content_length = 0
    if content_length > max_bytes:
        return _render_form(
            request,
            _msg("upload_too_large"),
            user=user,
        )
    try:
        form, files = request.multipart()
    except ValueError as exc:
        if str(exc) == "request_too_large":
            return _render_form(
                request,
                _msg("upload_too_large"),
                user=user,
            )
        raise
    if not auth.verify_csrf(request, form):
        return views.error_page("403 Forbidden", "forbidden")
    owner_token = _first(form, "owner")
    host = _first(form, "host")
    port_text = _first(form, "port")
    name = _first(form, "name")
    overview = _first(form, "overview")
    description = _first(form, "description")
    max_players = None
    num_players = None
    form_data = {
        "host": host,
        "port": port_text,
        "name": name,
        "overview": overview,
        "description": description,
    }
    if not host or not port_text or not name:
        return _render_form(
            request,
            _msg("missing_required"),
            user=user,
            form_data=form_data,
        )

    if overview and len(overview) > overview_max:
        warning_template = config.require_setting(
            config.require_setting(settings, "ui_text"),
            "warnings.overview_over_limit",
            "config.json ui_text.warnings",
        )
        return _render_form(
            request,
            _format_template(warning_template, max=overview_max),
            user=user,
            form_data=form_data,
        )

    try:
        port = int(port_text)
    except ValueError:
        return _render_form(
            request,
            _msg("port_number"),
            user=user,
            form_data=form_data,
        )

    with db.connect_ctx() as conn:
        existing = db.get_server_by_name(conn, name)
        if existing:
            return _render_form(
                request,
                _msg("name_taken"),
                user=user,
                form_data=form_data,
            )

    screenshot_id = None
    file_item = files.get("screenshot")
    if file_item is not None and file_item.filename:
        upload_info, error = uploads.handle_upload(file_item)
        if error:
            return _render_form(
                request,
                error,
                user=user,
                form_data=form_data,
            )
        screenshot_id = upload_info.get("id")

    owner_user_id = user["id"]
    if owner_token and auth.is_admin(user):
        with db.connect_ctx() as conn:
            owner_user = db.get_user_by_code(conn, owner_token)
            if not owner_user:
                owner_user = db.get_user_by_username(conn, owner_token)
            if not owner_user or owner_user["deleted"]:
                message = config.ui_text("messages.users.user_not_found")
                return _render_form(request, message, user=user, form_data=form_data, owner_token=owner_token)
            owner_user_id = owner_user["id"]

    record = {
        "name": name or None,
        "overview": overview or None,
        "description": description or None,
        "host": host,
        "port": port,
        "max_players": max_players,
        "num_players": num_players,
        "owner_user_id": owner_user_id,
        "screenshot_id": screenshot_id,
        "last_heartbeat": None,
    }

    with db.connect_ctx() as conn:
        db.add_server(conn, record)

        return _render_success(user=user, owner_token=owner_token if owner_user_id != user["id"] else "")
