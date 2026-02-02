import urllib.parse

from karma import auth, config, db, views, webhttp


def _first(form, key):
    value = form.get(key, [""])
    return value[0].strip()


def _row_value(row, key, default=""):
    try:
        value = row[key]
    except Exception:
        return default
    return value if value is not None else default


def _get_user_by_token(conn, token):
    if not token:
        return None
    user = db.get_user_by_code(conn, token)
    if user:
        return user
    return db.get_user_by_username(conn, token)


def _normalize_key(value):
    return value.strip().lower()


def _placeholder_attr(value):
    if not value:
        return ""
    return f' placeholder="{webhttp.html_escape(value)}"'


def _msg(key, **values):
    template = config.ui_text(f"messages.users.{key}", "config.json ui_text.messages.users")
    return config.format_text(template, **values)


def _label(key):
    return config.ui_text(f"labels.{key}", "config.json ui_text.labels")


def _action(key):
    return config.ui_text(f"actions.{key}", "config.json ui_text.actions")


def _title(key):
    return config.ui_text(f"titles.{key}", "config.json ui_text.titles")


def _section(key):
    return config.ui_text(f"sections.{key}", "config.json ui_text.sections")


def _status(key):
    return config.ui_text(f"status.{key}", "config.json ui_text.status")


def _is_admin(user):
    return auth.is_admin(user)


def _is_root_admin(user, settings):
    admin_username = config.require_setting(settings, "server.admin_user")
    return _normalize_key(user["username"]) == _normalize_key(admin_username)


def _admin_levels(conn, settings):
    admin_username = config.require_setting(settings, "server.admin_user")
    root_user = db.get_user_by_username(conn, admin_username)
    if not root_user:
        return {}, None
    levels = {root_user["id"]: 0}
    primary_admins = db.list_user_admins(conn, root_user["id"])
    for admin in primary_admins:
        levels[admin["admin_user_id"]] = 1
    for admin in primary_admins:
        if not admin["trust_admins"]:
            continue
        for sub_admin in db.list_user_admins(conn, admin["admin_user_id"]):
            levels.setdefault(sub_admin["admin_user_id"], 2)
    return levels, root_user["id"]


def _can_manage_user(current_user, target_user, levels, root_id):
    if root_id and current_user["id"] == root_id:
        return True
    current_level = levels.get(current_user["id"])
    target_level = levels.get(target_user["id"])
    if current_level is None:
        return False
    if target_level is None:
        return True
    return target_level >= current_level


def _render_users_list(
    request,
    users,
    current_user,
    message=None,
    form_data=None,
    show_admin_fields=False,
    root_admin_name="Admin",
    admin_levels=None,
    root_id=None,
):
    list_name = config.require_setting(config.get_config(), "server.community_name")
    form_data = form_data or {}

    csrf_html = views.csrf_input(auth.csrf_token(request))
    if show_admin_fields:
        rows = []
        for user in users:
            admin_flag = _status("yes") if user["is_admin"] else _status("no")
            locked = _status("yes") if user["is_locked"] else _status("no")
            deleted = _status("yes") if user["deleted"] else _status("no")
            is_root_target = _normalize_key(user["username"]) == _normalize_key(root_admin_name)
            can_lock = not is_root_target and (not user["is_admin"] or _is_root_admin(current_user, config.get_config()))
            can_edit = _can_manage_user(current_user, user, admin_levels or {}, root_id)
            lock_checked = "checked" if user["is_locked"] else ""
            lock_html = ""
            if can_lock:
                lock_html = f"""<form method="post" action="/users/lock" class="js-toggle-form">
      {csrf_html}
      <input type="hidden" name="id" value="{user["id"]}">
      <input type="checkbox" name="locked" value="1" {lock_checked}>
    </form>"""
            edit_html = ""
            if can_edit:
                token = auth.user_token(user)
                edit_html = f"""<a class="admin-link secondary" href="/users/{urllib.parse.quote(token, safe='')}/edit">{webhttp.html_escape(_action("edit"))}</a>"""
            rows.append(
                f"""<tr>
  <td><a class="plain-link bold-link" href="/users/{urllib.parse.quote(auth.user_token(user), safe='')}">{webhttp.html_escape(user["username"])}</a></td>
  <td class="center-cell"><span class="status-{admin_flag.lower()}">{admin_flag}</span></td>
  <td class="center-cell">
    {lock_html or f'<span class="status-{locked.lower()}">{locked}</span>'}
  </td>
  <td class="center-cell"><span class="status-{deleted.lower()}">{deleted}</span></td>
  <td class="center-cell">
    {edit_html}
  </td>
</tr>"""
            )
        empty_users = config.require_setting(config.get_config(), "ui_text.empty_states.users")
        rows_html = "".join(rows) or f"<tr><td colspan=\"5\">{webhttp.html_escape(empty_users)}</td></tr>"
        new_admin_checked = "checked" if form_data.get("is_admin") else ""
        show_admin_toggle = _is_root_admin(current_user, config.get_config())
        admin_toggle_html = ""
        if show_admin_toggle:
            admin_toggle_html = f"""  <div class="row">
    <div>
      <label for="new_is_admin">{webhttp.html_escape(_label("admin"))}</label>
      <input id="new_is_admin" name="is_admin" type="checkbox" {new_admin_checked}>
    </div>
  </div>
"""
        body = f"""<table class="users-table">
  <thead>
    <tr>
      <th class="center-cell">{webhttp.html_escape(_label("username"))}</th>
      <th class="center-cell">{webhttp.html_escape(_label("admin"))}</th>
      <th class="center-cell">{webhttp.html_escape(_label("lock"))}</th>
      <th class="center-cell">{webhttp.html_escape(_label("deleted"))}</th>
      <th class="center-cell">{webhttp.html_escape(_label("actions"))}</th>
    </tr>
  </thead>
  <tbody>
    {rows_html}
  </tbody>
</table>
<h2>{webhttp.html_escape(_section("add_user"))}</h2>
<form method="post" action="/users/create">
  {csrf_html}
  <div class="row">
    <div>
      <label for="new_username">{webhttp.html_escape(_label("username"))}</label>
      <input id="new_username" name="username" required value="{webhttp.html_escape(form_data.get("username", ""))}">
    </div>
    <div>
      <label for="new_email">{webhttp.html_escape(_label("email"))}</label>
      <input id="new_email" name="email" type="email" required value="{webhttp.html_escape(form_data.get("email", ""))}">
    </div>
  </div>
  <div class="row">
    <div>
      <label for="new_password">{webhttp.html_escape(_label("password"))}</label>
      <input id="new_password" name="password" type="password" required>
    </div>
  </div>
{admin_toggle_html}  <div class="actions">
    <button type="submit">{webhttp.html_escape(_action("create_user"))}</button>
  </div>
</form>
"""
    else:
        rows = "".join(
            f"""<tr>
  <td><a class="plain-link bold-link" href="/users/{urllib.parse.quote(auth.user_token(user), safe='')}">{webhttp.html_escape(user["username"])}</a></td>
</tr>"""
            for user in users
        )
        empty_users = config.require_setting(config.get_config(), "ui_text.empty_states.users")
        rows_html = rows or f"<tr><td>{webhttp.html_escape(empty_users)}</td></tr>"
        body = f"""<table>
  <thead>
    <tr>
      <th>{webhttp.html_escape(_label("username"))}</th>
    </tr>
  </thead>
  <tbody>
    {rows_html}
  </tbody>
</table>
"""

    profile_url = f"/users/{urllib.parse.quote(auth.user_token(current_user), safe='')}"
    header_html = views.header_with_title(
        list_name,
        "/users",
        logged_in=True,
        title=_title("users"),
        error=message,
        user_name=auth.display_username(current_user),
        is_admin=auth.is_admin(current_user),
        profile_url=profile_url,
    )
    return views.render_page_with_header(
        _title("users"),
        header_html,
        body,
        headers=views.no_store_headers(),
    )


def _render_user_edit(
    request,
    user,
    message=None,
    form_data=None,
    current_user=None,
    root_admin_name="Admin",
    admin_levels=None,
):
    form_data = form_data or {}
    placeholders = config.get_config().get("placeholders", {}).get("users", {})
    username_value = form_data.get("username", user["username"])
    email_value = form_data.get("email", user["email"])
    action_url = f"/users/{urllib.parse.quote(auth.user_token(user), safe='')}/edit"
    cancel_url = f"/users/{urllib.parse.quote(auth.user_token(user), safe='')}"
    csrf_html = views.csrf_input(auth.csrf_token(request))
    body = f"""<form method="post" action="{action_url}">
  {csrf_html}
  <input type="hidden" name="id" value="{user["id"]}">
  <div>
    <label for="username">{webhttp.html_escape(_label("username"))}</label>
    <input id="username" name="username" required value="{webhttp.html_escape(username_value)}">
  </div>
  <div>
    <label for="email">{webhttp.html_escape(_label("email"))}</label>
    <input id="email" name="email" type="email" required value="{webhttp.html_escape(email_value)}">
  </div>
  <div>
    <label for="password">{webhttp.html_escape(_label("reset_password_optional"))}</label>
    <input id="password" name="password" type="password"{_placeholder_attr(placeholders.get("reset_password"))}>
  </div>
  <div class="actions">
    <button type="submit">{webhttp.html_escape(_action("save_changes"))}</button>
    <a class="admin-link align-right" href="{cancel_url}">{webhttp.html_escape(_action("cancel"))}</a>
  </div>
</form>
"""
    if auth.is_admin(current_user):
        action_label = _action("delete_user")
        action_class = "danger"
        action_url = "/users/delete"
        confirm_text = config.ui_text("confirmations.delete_user")
        confirm_label = _action("delete")
        confirm_style = "danger"
        if user["deleted"]:
            action_label = _action("reinstate_user")
            action_class = "success"
            action_url = "/users/reinstate"
            confirm_text = config.ui_text("confirmations.reinstate_user")
            confirm_label = _action("reinstate")
            confirm_style = "success"
        body += f"""<form method="post" action="{action_url}" data-confirm="{confirm_text}" data-confirm-label="{confirm_label}" data-confirm-style="{confirm_style}">
  {csrf_html}
  <input type="hidden" name="id" value="{user["id"]}">
  <div class="actions center">
    <button type="submit" class="{action_class}">{webhttp.html_escape(action_label)}</button>
  </div>
</form>
"""
    profile_url = f"/users/{urllib.parse.quote(auth.user_token(current_user), safe='')}"
    header_html = views.header_with_title(
        config.require_setting(config.get_config(), "server.community_name"),
        "/users/edit",
        logged_in=True,
        title=_title("edit_user"),
        error=message,
        user_name=auth.display_username(current_user),
        is_admin=auth.is_admin(current_user),
        profile_url=profile_url,
    )
    return views.render_page_with_header(
        _title("edit_user"),
        header_html,
        body,
        headers=views.no_store_headers(),
    )


def _render_user_settings(request, user, message=None, form_data=None, current_user=None):
    form_data = form_data or {}
    placeholders = config.get_config().get("placeholders", {}).get("users", {})
    email_value = form_data.get("email", user["email"])
    language_value = form_data.get("language", _row_value(user, "language"))
    language_options = []
    languages = config.get_available_languages()
    language_labels = config.get_config().get("languages", {})
    auto_label = _label("language_auto")
    language_options.append(("", auto_label))
    for code in languages:
        label = language_labels.get(code, code)
        language_options.append((code, label))
    csrf_html = views.csrf_input(auth.csrf_token(request))
    language_options_html = []
    for code, label in language_options:
        selected = " selected" if code == language_value else ""
        language_options_html.append(
            f'<option value="{webhttp.html_escape(code)}"{selected}>{webhttp.html_escape(label)}</option>'
        )
    body = f"""<form method="post" action="/users/{urllib.parse.quote(auth.user_token(user), safe='')}/edit">
  {csrf_html}
  <div class="row">
    <div>
      <label for="email">{webhttp.html_escape(_label("email"))}</label>
      <input id="email" name="email" type="email" required value="{webhttp.html_escape(email_value)}">
    </div>
    <div>
      <label for="language">{webhttp.html_escape(_label("language"))}</label>
      <select id="language" name="language">
        {"".join(language_options_html)}
      </select>
    </div>
  </div>
  <div>
    <label for="password">{webhttp.html_escape(_label("new_password_optional"))}</label>
    <input id="password" name="password" type="password"{_placeholder_attr(placeholders.get("new_password"))}>
  </div>
  <div class="actions">
    <button type="submit">{webhttp.html_escape(_action("save_changes"))}</button>
    <a class="admin-link align-right" href="/users/{urllib.parse.quote(auth.user_token(user), safe='')}">{webhttp.html_escape(_action("cancel"))}</a>
  </div>
</form>
"""
    profile_url = f"/users/{urllib.parse.quote(auth.user_token(current_user), safe='')}"
    header_html = views.header_with_title(
        config.require_setting(config.get_config(), "server.community_name"),
        f"/users/{urllib.parse.quote(auth.user_token(user), safe='')}/edit",
        logged_in=True,
        title=_title("personal_settings"),
        error=message,
        user_name=auth.display_username(current_user),
        is_admin=auth.is_admin(current_user),
        profile_url=profile_url,
    )
    return views.render_page_with_header(
        _title("personal_settings"),
        header_html,
        body,
        headers=views.no_store_headers(),
    )


def _handle_admin_edit(
    request,
    conn,
    current_user,
    target_user,
    form,
    admin_username,
    admin_levels,
    root_id,
    is_root_admin,
    redirect_url="/users",
):
    is_root_target = _normalize_key(target_user["username"]) == _normalize_key(admin_username)
    if not is_root_admin and is_root_target:
        return webhttp.redirect(redirect_url)
    if not _can_manage_user(current_user, target_user, admin_levels, root_id):
        return webhttp.redirect(redirect_url)
    username = _first(form, "username")
    email = _first(form, "email").lower()
    password = _first(form, "password")
    is_admin_value = _first(form, "is_admin") == "on"
    form_data = {"username": username, "email": email, "is_admin": is_admin_value}
    if not username or not email:
        return _render_user_edit(
            request,
            target_user,
            _msg("username_email_required"),
            form_data=form_data,
            current_user=current_user,
            root_admin_name=admin_username,
            admin_levels=admin_levels,
        )
    if " " in username:
        return _render_user_edit(
            request,
            target_user,
            _msg("username_spaces"),
            form_data=form_data,
            current_user=current_user,
            root_admin_name=admin_username,
            admin_levels=admin_levels,
        )
    if _normalize_key(username) == _normalize_key(admin_username):
        return _render_user_edit(
            request,
            target_user,
            _msg("username_reserved"),
            form_data=form_data,
            current_user=current_user,
            root_admin_name=admin_username,
            admin_levels=admin_levels,
        )
    if is_root_target and _normalize_key(username) != _normalize_key(admin_username):
        return _render_user_edit(
            request,
            target_user,
            _msg("root_username_locked"),
            form_data=form_data,
            current_user=current_user,
            root_admin_name=admin_username,
            admin_levels=admin_levels,
        )
    existing = db.get_user_by_username(conn, username)
    if existing and existing["id"] != target_user["id"]:
        return _render_user_edit(
            request,
            target_user,
            _msg("username_taken"),
            form_data=form_data,
            current_user=current_user,
            root_admin_name=admin_username,
            admin_levels=admin_levels,
        )
    if email != target_user["email"]:
        existing_email = db.get_user_by_email(conn, email)
        if existing_email and existing_email["id"] != target_user["id"]:
            return _render_user_edit(
                request,
                target_user,
                _msg("email_in_use"),
                form_data=form_data,
                current_user=current_user,
                root_admin_name=admin_username,
                admin_levels=admin_levels,
            )
    db.update_user_email(conn, target_user["id"], email)
    if username != target_user["username"]:
        db.update_user_username(conn, target_user["id"], username)
    if is_root_admin:
        if is_root_target:
            root_user = db.get_user_by_username(conn, admin_username)
            if root_user:
                db.recompute_admin_flags(conn, root_user["id"])
        else:
            root_user = db.get_user_by_username(conn, admin_username)
            if root_user:
                if is_admin_value:
                    db.add_user_admin(conn, root_user["id"], target_user["id"])
                else:
                    db.remove_user_admin(conn, root_user["id"], target_user["id"])
                db.recompute_admin_flags(conn, root_user["id"])
    if password:
        digest, salt = auth.new_password(password)
        db.set_user_password(conn, target_user["id"], digest, salt)
    return webhttp.redirect(redirect_url)


def handle(request):
    settings = config.get_config()
    current_user = auth.get_user_from_request(request)
    if not current_user:
        return webhttp.redirect("/login")
    is_admin = _is_admin(current_user)
    admin_username = config.require_setting(settings, "server.admin_user")
    is_root_admin = _is_root_admin(current_user, settings)

    path = request.path.rstrip("/") or "/users"
    with db.connect_ctx() as conn:
        admin_levels, root_id = _admin_levels(conn, settings)
        if path.startswith("/users/") and path.endswith("/edit"):
            token = request.query.get("token", [""])[0].strip()
            username = request.query.get("name", [""])[0].strip()
            lookup = token or username
            if not lookup:
                return views.error_page("400 Bad Request", "missing_user")
            target_user = _get_user_by_token(conn, lookup)
            if request.method == "POST" and not target_user:
                form = request.form()
                user_id = _first(form, "id")
                if user_id.isdigit():
                    target_user = db.get_user_by_id(conn, int(user_id))
            if not target_user:
                return views.error_page("404 Not Found", "user_not_found")
            if current_user["id"] == target_user["id"]:
                if request.method == "GET":
                    return _render_user_settings(request, target_user, current_user=current_user)
                if request.method == "POST":
                    form = request.form()
                    if not auth.verify_csrf(request, form):
                        return views.error_page("403 Forbidden", "forbidden")
                    email = _first(form, "email").lower()
                    password = _first(form, "password")
                    language = _first(form, "language")
                    form_data = {"email": email, "language": language}
                    if not email:
                        return _render_user_settings(
                            request,
                            target_user,
                            _msg("email_required"),
                            form_data=form_data,
                            current_user=current_user,
                        )
                    language = config.normalize_language(language)
                    if language:
                        available = config.get_available_languages()
                        if language not in available:
                            return _render_user_settings(
                                request,
                                target_user,
                                _msg("language_invalid"),
                                form_data=form_data,
                                current_user=current_user,
                            )
                    if email != target_user["email"]:
                        existing_email = db.get_user_by_email(conn, email)
                        if existing_email and existing_email["id"] != target_user["id"]:
                            return _render_user_settings(
                                request,
                                target_user,
                                _msg("email_in_use"),
                                form_data=form_data,
                                current_user=current_user,
                            )
                    db.update_user_email(conn, target_user["id"], email)
                    db.update_user_language(conn, target_user["id"], language or None)
                    if password:
                        digest, salt = auth.new_password(password)
                        db.set_user_password(conn, target_user["id"], digest, salt)
                    return webhttp.redirect(f"/users/{urllib.parse.quote(auth.user_token(target_user), safe='')}")
                return views.error_page("405 Method Not Allowed", "method_not_allowed")
            if not is_admin:
                return views.error_page("403 Forbidden", "forbidden")
            if not _can_manage_user(current_user, target_user, admin_levels, root_id):
                return views.error_page("403 Forbidden", "forbidden")
            if request.method == "GET":
                return _render_user_edit(
                    request,
                    target_user,
                    current_user=current_user,
                    root_admin_name=admin_username,
                    admin_levels=admin_levels,
                )
            if request.method == "POST":
                return _handle_admin_edit(
                    request,
                    conn,
                    current_user,
                    target_user,
                    request.form(),
                    admin_username,
                    admin_levels,
                    root_id,
                    is_root_admin,
                    redirect_url=f"/users/{urllib.parse.quote(auth.user_token(target_user), safe='')}",
                )
            return views.error_page("405 Method Not Allowed", "method_not_allowed")
        if path in ("/users", "/users/"):
            users = db.list_users(conn)
            return _render_users_list(
                request,
                users,
                current_user,
                show_admin_fields=is_admin,
                root_admin_name=admin_username,
                admin_levels=admin_levels,
                root_id=root_id,
            )

        if path in ("/users/create", "/users/create/") and request.method == "POST":
            if not is_admin:
                return webhttp.redirect("/users")
            form = request.form()
            if not auth.verify_csrf(request, form):
                return views.error_page("403 Forbidden", "forbidden")
            username = _first(form, "username")
            email = _first(form, "email").lower()
            password = _first(form, "password")
            is_admin_value = _first(form, "is_admin") == "on"
            form_data = {
                "username": username,
                "email": email,
                "is_admin": is_admin_value,
            }
            if not username or not email or not password:
                users = db.list_users(conn)
                return _render_users_list(
                    request,
                    users,
                    current_user,
                    message=_msg("create_missing_fields"),
                    form_data=form_data,
                    show_admin_fields=True,
                    root_admin_name=admin_username,
                )
            if " " in username:
                users = db.list_users(conn)
                return _render_users_list(
                    request,
                    users,
                    current_user,
                    message=_msg("username_spaces"),
                    form_data=form_data,
                    show_admin_fields=True,
                    root_admin_name=admin_username,
                )
            if _normalize_key(username) == _normalize_key(admin_username):
                users = db.list_users(conn)
                return _render_users_list(
                    request,
                    users,
                    current_user,
                    message=_msg("username_reserved"),
                    form_data=form_data,
                    show_admin_fields=True,
                    root_admin_name=admin_username,
                )
            if db.get_user_by_username(conn, username):
                users = db.list_users(conn)
                return _render_users_list(
                    request,
                    users,
                    current_user,
                    message=_msg("username_taken"),
                    form_data=form_data,
                    show_admin_fields=True,
                    root_admin_name=admin_username,
                )
            if db.get_user_by_email(conn, email):
                users = db.list_users(conn)
                return _render_users_list(
                    request,
                    users,
                    current_user,
                    message=_msg("email_registered"),
                    form_data=form_data,
                    show_admin_fields=True,
                    root_admin_name=admin_username,
                )
            digest, salt = auth.new_password(password)
            db.add_user(conn, username, email, digest, salt, is_admin=False, is_admin_manual=False)
            root_user = db.get_user_by_username(conn, admin_username)
            if root_user:
                if is_root_admin and is_admin_value:
                    new_user = db.get_user_by_username(conn, username)
                    if new_user:
                        db.add_user_admin(conn, root_user["id"], new_user["id"])
                db.recompute_admin_flags(conn, root_user["id"])
            return webhttp.redirect("/users")

        if path in ("/users/lock", "/users/lock/") and request.method == "POST":
            if not is_admin:
                return webhttp.redirect("/users")
            form = request.form()
            if not auth.verify_csrf(request, form):
                return views.error_page("403 Forbidden", "forbidden")
            user_id = _first(form, "id")
            if not user_id.isdigit():
                return webhttp.redirect("/users")
            user = db.get_user_by_id(conn, int(user_id))
            if not user:
                return webhttp.redirect("/users")
            if _normalize_key(user["username"]) == _normalize_key(admin_username):
                return webhttp.redirect("/users")
            if user["is_admin"] and not is_root_admin:
                return webhttp.redirect("/users")
            locked = _first(form, "locked") == "1"
            db.set_user_locked(conn, int(user_id), locked)
            return webhttp.redirect("/users")

        if path in ("/users/edit", "/users/edit/"):
            if not is_admin:
                return webhttp.redirect("/users")
            if request.method == "GET":
                user_id = request.query.get("id", [""])[0]
                if user_id.isdigit():
                    user = db.get_user_by_id(conn, int(user_id))
                    if user:
                        if not is_root_admin and _normalize_key(user["username"]) == _normalize_key(admin_username):
                            return webhttp.redirect("/users")
                        if not _can_manage_user(current_user, user, admin_levels, root_id):
                            return webhttp.redirect("/users")
                        return _render_user_edit(
                            request,
                            user,
                            current_user=current_user,
                            root_admin_name=admin_username,
                            admin_levels=admin_levels,
                        )
                return webhttp.redirect("/users")
            if request.method == "POST":
                form = request.form()
                if not auth.verify_csrf(request, form):
                    return views.error_page("403 Forbidden", "forbidden")
                user_id = _first(form, "id")
                if not user_id.isdigit():
                    return webhttp.redirect("/users")
                user = db.get_user_by_id(conn, int(user_id))
                if not user:
                    return webhttp.redirect("/users")
                return _handle_admin_edit(
                    request,
                    conn,
                    current_user,
                    user,
                    form,
                    admin_username,
                    admin_levels,
                    root_id,
                    is_root_admin,
                )

        if path in ("/users/delete", "/users/delete/") and request.method == "POST":
            if not is_admin:
                return webhttp.redirect("/users")
            form = request.form()
            if not auth.verify_csrf(request, form):
                return views.error_page("403 Forbidden", "forbidden")
            user_id = _first(form, "id")
            if user_id.isdigit():
                user = db.get_user_by_id(conn, int(user_id))
                if user:
                    if _normalize_key(user["username"]) == _normalize_key(admin_username):
                        return webhttp.redirect("/users")
                    if not is_root_admin and user["is_admin"]:
                        return webhttp.redirect("/users")
                    if not _can_manage_user(current_user, user, admin_levels, root_id):
                        return webhttp.redirect("/users")
                    db.set_user_deleted(conn, int(user_id), True)
            return webhttp.redirect("/users")

        if path in ("/users/reinstate", "/users/reinstate/") and request.method == "POST":
            if not is_admin:
                return webhttp.redirect("/users")
            form = request.form()
            if not auth.verify_csrf(request, form):
                return views.error_page("403 Forbidden", "forbidden")
            user_id = _first(form, "id")
            if user_id.isdigit():
                user = db.get_user_by_id(conn, int(user_id))
                if user:
                    if _normalize_key(user["username"]) == _normalize_key(admin_username):
                        return webhttp.redirect("/users")
                    if not is_root_admin and user["is_admin"]:
                        return webhttp.redirect("/users")
                    if not _can_manage_user(current_user, user, admin_levels, root_id):
                        return webhttp.redirect("/users")
                    db.set_user_deleted(conn, int(user_id), False)
            return webhttp.redirect("/users")
    return views.error_page("404 Not Found", "not_found")
