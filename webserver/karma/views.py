import json
import urllib.parse

from karma import config, webhttp


def _ui_text(path):
    return config.ui_text(path)


def _nav(path):
    return config.ui_text(f"nav.{path}")


def _label(path):
    return config.ui_text(f"labels.{path}")


def _action(path):
    return config.ui_text(f"actions.{path}")


def _title(path):
    return config.ui_text(f"titles.{path}")


def _section(path):
    return config.ui_text(f"sections.{path}")


def _status(path):
    return config.ui_text(f"status.{path}")


def _confirm(path):
    return config.ui_text(f"confirm.{path}")


def _confirm_text(path):
    return config.ui_text(f"confirmations.{path}")


def _template(path):
    return config.ui_text(f"templates.{path}")


def _header_text(path):
    return config.ui_text(f"header.{path}")


def error_page(status, key, message=None):
    error_cfg = _ui_text(f"errors.{key}")
    generic_cfg = _ui_text("errors.generic")
    if isinstance(generic_cfg, dict):
        generic_title = generic_cfg.get("title") or ""
        generic_message = generic_cfg.get("message") or ""
    else:
        generic_title = str(generic_cfg)
        generic_message = ""
    if isinstance(error_cfg, dict):
        title = error_cfg.get("title") or generic_title
        if message is not None:
            body_message = message
        else:
            body_message = error_cfg.get("message") or generic_message
    else:
        title = str(error_cfg) or generic_title
        body_message = message or generic_message
    body = f"<h1>{webhttp.html_escape(title)}</h1>"
    if body_message:
        body += f"<p class=\"muted\">{webhttp.html_escape(body_message)}</p>"
    _, headers, response_body = render_page(title, body, message=None)
    return status, headers, response_body


def render_page(title, body_html, message=None, header_links_html=None, headers=None):
    nav_html = ""
    if header_links_html:
        nav_html = f"""
    <div class="nav-strip">
      <div class="header-links">
        {header_links_html}
      </div>
    </div>
"""
    overview_empty = _ui_text("empty_states.overview")
    servers_empty = _ui_text("empty_states.servers")
    overview_empty_js = json.dumps(f'<p class="muted">{overview_empty}</p>')
    servers_empty_js = json.dumps(f'<p class="muted">{servers_empty}</p>')
    confirm_title = webhttp.html_escape(_confirm("title"))
    confirm_message = webhttp.html_escape(_confirm("message"))
    confirm_cancel = webhttp.html_escape(_confirm("cancel_label"))
    confirm_ok = webhttp.html_escape(_confirm("ok_label"))
    confirm_html = """
    <div class="confirm-modal" id="confirm-modal" hidden>
      <div class="confirm-card" role="dialog" aria-modal="true" aria-labelledby="confirm-title">
        <h3 id="confirm-title">__CONFIRM_TITLE__</h3>
        <p id="confirm-message">__CONFIRM_MESSAGE__</p>
        <div class="actions">
          <button type="button" class="secondary" id="confirm-cancel">__CONFIRM_CANCEL__</button>
          <button type="button" id="confirm-ok">__CONFIRM_OK__</button>
        </div>
      </div>
    </div>
    <script>
      (() => {
        const modal = document.getElementById('confirm-modal');
        if (!modal) return;
        const message = document.getElementById('confirm-message');
        const cancel = document.getElementById('confirm-cancel');
        const ok = document.getElementById('confirm-ok');
        let pendingForm = null;
        document.addEventListener('submit', (event) => {
          const form = event.target;
          if (!(form instanceof HTMLFormElement)) return;
          const text = form.getAttribute('data-confirm');
          if (!text) return;
          event.preventDefault();
          pendingForm = form;
          message.textContent = text;
          const label = form.getAttribute('data-confirm-label');
          const style = form.getAttribute('data-confirm-style');
          if (label) {
            ok.textContent = label;
          } else {
            ok.textContent = '__CONFIRM_OK__';
          }
          ok.classList.remove('danger', 'success');
          if (style === 'success') {
            ok.classList.add('success');
          } else if (style === 'danger') {
            ok.classList.add('danger');
          }
          modal.hidden = false;
          cancel.focus();
        });
        cancel.addEventListener('click', () => {
          modal.hidden = true;
          pendingForm = null;
        });
        ok.addEventListener('click', () => {
          if (pendingForm) {
            pendingForm.submit();
          }
          modal.hidden = true;
          pendingForm = null;
        });
        modal.addEventListener('click', (event) => {
          if (event.target === modal) {
            modal.hidden = true;
            pendingForm = null;
          }
        });
        document.addEventListener('keydown', (event) => {
          if (event.key === 'Escape' && !modal.hidden) {
            modal.hidden = true;
            pendingForm = null;
          }
        });
      })();
    </script>
    <script>
      (() => {
        document.addEventListener('change', (event) => {
          const input = event.target;
          if (!(input instanceof HTMLInputElement) || input.type !== 'checkbox') return;
          const form = input.closest('form.js-toggle-form');
          if (!form) return;
          event.preventDefault();
          const formData = new FormData(form);
          input.disabled = true;
          fetch(form.action, {
            method: (form.method || 'post').toUpperCase(),
            body: formData,
            credentials: 'same-origin',
          })
            .then((response) => {
              if (!response.ok) {
                throw new Error('toggle failed');
              }
            })
            .catch(() => {
              input.checked = !input.checked;
            })
            .finally(() => {
              input.disabled = false;
            });
        });
      })();
    </script>
    <script>
      (() => {
        const escapeHtml = (value) => {
          if (value === null || value === undefined) return "";
          return String(value)
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/"/g, "&quot;")
            .replace(/'/g, "&#39;");
        };
        const buildCardElement = (server, allowActions, csrfToken) => {
          const rawName = server.name || `${server.host}:${server.port}`;
          const name = escapeHtml(rawName);
          const serverToken = server.code || rawName;
          const serverUrl = `/server/${encodeURIComponent(serverToken)}`;
          const owner = escapeHtml(server.owner || "");
          const ownerToken = server.owner_code || server.owner || "";
          const ownerUrl = `/users/${encodeURIComponent(ownerToken)}`;
          const overviewText = server.overview || "";
          const description = overviewText
            ? `<p>${escapeHtml(overviewText)}</p>`
            : __OVERVIEW_EMPTY__;
          const hasScreenshot = Boolean(server.screenshot_id);
          const cardClass = hasScreenshot ? "card-top" : "card-top no-thumb";
          const screenshotHtml = hasScreenshot
            ? `<img class="thumb" src="/uploads/${escapeHtml(server.screenshot_id)}_thumb.jpg" data-full="/uploads/${escapeHtml(server.screenshot_id)}_full.jpg" alt="__SCREENSHOT_ALT__">`
            : "";
          let onlineValue = "—";
          if (server.num_players !== undefined && server.num_players !== null && server.max_players !== undefined && server.max_players !== null) {
            onlineValue = `${server.num_players} / ${server.max_players}`;
          } else if (server.num_players !== undefined && server.num_players !== null) {
            onlineValue = `${server.num_players}`;
          } else if (server.max_players !== undefined && server.max_players !== null) {
            onlineValue = `— / ${server.max_players}`;
          }
          const onlineLabel = server.active
            ? `<span class="online-label">__ONLINE_PREFIX__</span> <span class="online-value">${escapeHtml(onlineValue)}</span>`
            : '<span class="online-label inactive">__OFFLINE_LABEL__</span>';
          const onlineClass = server.active ? "online active" : "online inactive";
          const endpointHtml = server.active
            ? `<div class="endpoint">${escapeHtml(server.host)}:${escapeHtml(server.port)}</div>`
            : "";
          let actionsBlock = "";
          if (allowActions && server.id !== undefined && server.id !== null) {
            const editToken = server.code || rawName;
            const csrf = csrfToken ? `<input type="hidden" name="csrf_token" value="${escapeHtml(csrfToken)}">` : "";
            actionsBlock = `<div class="card-actions">
  <a class="button-link secondary small" href="/server/${encodeURIComponent(editToken)}/edit">__ACTION_EDIT__</a>
  <form method="post" action="/server/delete" data-confirm="__CONFIRM_DELETE_SERVER__">
    ${csrf}
    <input type="hidden" name="id" value="${escapeHtml(server.id)}">
    <button type="submit" class="secondary small">__ACTION_DELETE__</button>
  </form>
</div>`;
          }
          const html = `<article class="card">
  <header>
    <div class="title-block">
      <h3><a class="server-link" href="${serverUrl}">${name}</a></h3>
      <div class="owner-line">__OWNER_BY__ <a class="owner-link" href="${ownerUrl}">${owner}</a></div>
    </div>
    <div class="online-block">
      <div class="${onlineClass}">${onlineLabel}</div>
      ${endpointHtml}
      ${actionsBlock}
    </div>
  </header>
  <div class="${cardClass}">
    ${screenshotHtml}
    <div class="card-details">
      ${description}
    </div>
  </div>
</article>`;
          const template = document.createElement("template");
          template.innerHTML = html.trim();
          return template.content.firstElementChild;
        };
        const renderCards = (section, servers) => {
          const container = section.querySelector('[data-role="server-cards"]');
          if (!container) return;
          const animate = section.dataset.animate === "1";
          const csrfToken = section.dataset.csrfToken || "";
          if (!servers.length) {
            container.innerHTML = __SERVERS_EMPTY__;
            return;
          }
          const grid = container.querySelector(".card-grid") || document.createElement("div");
          grid.className = "card-grid";
          const existingCards = {};
          grid.querySelectorAll(".card[data-server-id]").forEach((card) => {
            existingCards[card.dataset.serverId] = card;
          });
          const oldRects = {};
          if (animate) {
            Object.values(existingCards).forEach((card) => {
              oldRects[card.dataset.serverId] = card.getBoundingClientRect();
            });
          }
          const allowActions = section.dataset.allowActions === "1";
          const newCards = [];
          servers.forEach((server) => {
            const key = String(server.id ?? (server.name || `${server.host}:${server.port}`));
            let card = existingCards[key];
            const nextCard = buildCardElement(server, allowActions, csrfToken);
            nextCard.dataset.serverId = key;
            if (card) {
              card.dataset.serverId = key;
              card.innerHTML = nextCard.innerHTML;
              newCards.push(card);
            } else {
              newCards.push(nextCard);
            }
          });
          grid.replaceChildren(...newCards);
          if (!container.contains(grid)) {
            container.replaceChildren(grid);
          }
          if (animate) {
            const movers = [];
            grid.querySelectorAll(".card[data-server-id]").forEach((card) => {
              const prev = oldRects[card.dataset.serverId];
              if (!prev) return;
              const next = card.getBoundingClientRect();
              const dx = prev.left - next.left;
              const dy = prev.top - next.top;
              if (!dx && !dy) return;
              card.style.transform = `translate(${dx}px, ${dy}px)`;
              movers.push(card);
            });
            if (movers.length) {
              movers.forEach((card) => {
                void card.offsetHeight;
              });
              requestAnimationFrame(() => {
                movers.forEach((card) => {
                  card.style.transition = "transform 420ms ease";
                  card.style.transform = "";
                });
                const cleanup = () => {
                  movers.forEach((card) => {
                    card.style.transition = "";
                  });
                };
                setTimeout(cleanup, 460);
              });
            }
          }
        };
        const formatSummary = (template, activeCount, inactiveCount) =>
          String(template || "")
            .replace(/\\{active\\}/g, String(activeCount))
            .replace(/\\{inactive\\}/g, String(inactiveCount));
        const updateSummary = (section, activeCount, inactiveCount) => {
          const summary = section.querySelector('[data-role="server-summary"]');
          if (!summary) return;
          const template = summary.dataset.summaryTemplate || "";
          if (!template) return;
          summary.innerHTML = formatSummary(template, activeCount, inactiveCount);
        };
        const refreshSection = (section) => {
          const url = section.dataset.refreshUrl;
          if (!url) return;
          fetch(url, { credentials: "same-origin" })
            .then((response) => response.json())
            .then((payload) => {
              if (!payload || !Array.isArray(payload.servers)) return;
              renderCards(section, payload.servers);
              if (typeof payload.active_count === "number" && typeof payload.inactive_count === "number") {
                updateSummary(section, payload.active_count, payload.inactive_count);
              }
            })
            .catch(() => {});
        };
        document.querySelectorAll("[data-refresh-url]").forEach((section) => {
          const interval = parseInt(section.dataset.refreshInterval || "10", 10);
          refreshSection(section);
          setInterval(() => refreshSection(section), interval * 1000);
        });
      })();
    </script>
    """
    confirm_html = confirm_html.replace("__OVERVIEW_EMPTY__", overview_empty_js)
    confirm_html = confirm_html.replace("__SERVERS_EMPTY__", servers_empty_js)
    confirm_html = confirm_html.replace("__OWNER_BY__", webhttp.html_escape(_label("owner_by")))
    confirm_html = confirm_html.replace("__ONLINE_PREFIX__", webhttp.html_escape(_status("online_prefix")))
    confirm_html = confirm_html.replace("__OFFLINE_LABEL__", webhttp.html_escape(_status("offline")))
    confirm_html = confirm_html.replace("__CONFIRM_DELETE_SERVER__", webhttp.html_escape(_confirm_text("delete_server")))
    confirm_html = confirm_html.replace("__SCREENSHOT_ALT__", webhttp.html_escape(_label("screenshot_alt")))
    confirm_html = confirm_html.replace("__ACTION_EDIT__", webhttp.html_escape(_action("edit")))
    confirm_html = confirm_html.replace("__ACTION_DELETE__", webhttp.html_escape(_action("delete")))
    confirm_html = confirm_html.replace("__CONFIRM_TITLE__", confirm_title)
    confirm_html = confirm_html.replace("__CONFIRM_MESSAGE__", confirm_message)
    confirm_html = confirm_html.replace("__CONFIRM_CANCEL__", confirm_cancel)
    confirm_html = confirm_html.replace("__CONFIRM_OK__", confirm_ok)
    confirm_html += """
    <script>
      (() => {
        const formatTemplate = (template, count, max) =>
          String(template || "")
            .replace(/\\{count\\}/g, String(count))
            .replace(/\\{max\\}/g, String(max));
        const fields = document.querySelectorAll("[data-char-limit]");
        fields.forEach((field) => {
          const wrapper = field.closest("[data-char-field]") || field.parentElement;
          if (!wrapper) return;
          const counter = wrapper.querySelector(".char-counter");
          const warning = wrapper.querySelector(".char-warning");
          const max = parseInt(field.dataset.charLimit || "0", 10);
          const counterTemplate = field.dataset.counterTemplate || "";
          const warningTemplate = field.dataset.warningTemplate || "";
          const update = () => {
            const count = field.value.length;
            if (counter) {
              counter.textContent = formatTemplate(counterTemplate, count, max);
            }
            if (!warning) return;
            if (max && count > max) {
              warning.textContent = formatTemplate(warningTemplate, count, max);
              warning.classList.remove("hidden");
              field.classList.add("field-error");
            } else {
              warning.classList.add("hidden");
              field.classList.remove("field-error");
            }
          };
          field.addEventListener("input", update);
          update();
        });
      })();
    </script>
    """
    settings = config.get_config()
    lang = webhttp.html_escape((settings.get("server") or {}).get("language") or "en")
    direction = settings.get("meta", {}).get("direction", "ltr")
    direction = "rtl" if str(direction).lower() == "rtl" else "ltr"
    html = f"""<!doctype html>
<html lang="{lang}" dir="{direction}">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>{webhttp.html_escape(title)}</title>
  <link rel="stylesheet" href="/static/style.css">
</head>
<body>
  <div class="container">
    {nav_html}
    {body_html}
  </div>
  {confirm_html}
</body>
</html>
"""
    return webhttp.html_response(html, headers=headers)


def no_store_headers():
    return [("Cache-Control", "no-store"), ("Pragma", "no-cache")]


def render_page_with_header(title, header_html, body_html, message=None, header_links_html=None, headers=None):
    full_body = f"""{header_html}
{body_html}"""
    return render_page(title, full_body, message=message, header_links_html=header_links_html, headers=headers)


def csrf_input(token):
    safe_token = webhttp.html_escape(token or "")
    return f'<input type="hidden" name="csrf_token" value="{safe_token}">'


def header(
    list_name,
    current_path,
    logged_in,
    show_login=True,
    info=None,
    warning=None,
    error=None,
    logout_url="/logout",
    user_name=None,
    is_admin=False,
    profile_url=None,
):
    links = []
    user_html = ""
    if logged_in and user_name:
        safe_name = webhttp.html_escape(user_name)
        signed_in_as = webhttp.html_escape(_header_text("signed_in_as"))
        if profile_url:
            user_html = (
                f'<div class="header-user">{signed_in_as} '
                f'<a class="header-username" href="{profile_url}">{safe_name}</a></div>'
            )
        else:
            user_html = f'<div class="header-user">{signed_in_as} <span class="header-username">{safe_name}</span></div>'
    if current_path != "/info":
        links.append(f'<a class="admin-link" href="/info">{webhttp.html_escape(_nav("info"))}</a>')
    if current_path != "/servers/active" and not current_path.startswith("/servers/"):
        links.append(f'<a class="admin-link" href="/servers/active">{webhttp.html_escape(_nav("servers"))}</a>')
    if logged_in and is_admin and current_path != "/users":
        links.append(f'<a class="admin-link" href="/users">{webhttp.html_escape(_nav("users"))}</a>')
    if logged_in:
        links.append(f'<a class="admin-link secondary" href="{logout_url}">{webhttp.html_escape(_nav("logout"))}</a>')
    elif show_login:
        links.append(f'<a class="admin-link secondary" href="/login">{webhttp.html_escape(_nav("login"))}</a>')
    links_html = "".join(links)
    notice_html = ""
    if info:
        notice_html = f'<div class="notice notice-info">{webhttp.html_escape(info)}</div>'
    elif warning:
        notice_html = f'<div class="notice notice-warning">{webhttp.html_escape(warning)}</div>'
    elif error:
        notice_html = f'<div class="notice notice-error">{webhttp.html_escape(error)}</div>'
    return f"""<div class="page-header">
  <h1 class="site-title">{webhttp.html_escape(list_name)}</h1>
  <div class="header-actions">
    <div class="header-links">
      {links_html}
    </div>
    {user_html}
  </div>
</div>
<hr class="section-divider">
{notice_html}"""


def header_with_title(
    list_name,
    current_path,
    logged_in,
    title,
    show_login=True,
    info=None,
    warning=None,
    error=None,
    logout_url="/logout",
    user_name=None,
    is_admin=False,
    profile_url=None,
):
    header_html = header(
        list_name,
        current_path,
        logged_in,
        show_login=show_login,
        info=info,
        warning=warning,
        error=error,
        logout_url=logout_url,
        user_name=user_name,
        is_admin=is_admin,
        profile_url=profile_url,
    )
    return f"""{header_html}
<h2>{webhttp.html_escape(title)}</h2>"""


def render_server_cards(
    servers,
    header_title=None,
    summary_text=None,
    toggle_url=None,
    toggle_label=None,
    header_title_html=None,
    header_actions_html=None,
):
    from karma import lightbox, uploads

    header_html = ""
    if header_title or header_title_html:
        title_html = header_title_html or webhttp.html_escape(header_title)
        summary_template = _template("server_summary")
        summary_html = (
            f'<div class="summary server-summary" data-role="server-summary" '
            f'data-summary-template="{webhttp.html_escape(summary_template)}">{summary_text}</div>'
            if summary_text
            else ""
        )
        toggle_html = f'<a class="admin-link secondary" href="{toggle_url}">{toggle_label}</a>' if toggle_url else ""
        actions_html = header_actions_html if header_actions_html else toggle_html
        header_html = f"""<div class="section-header">
  <h2>{title_html}</h2>
  {summary_html}
  {actions_html}
</div>
"""

    cards = []
    for entry in servers:
        raw_name = entry.get("name") or f"{entry['host']}:{entry['port']}"
        name = webhttp.html_escape(raw_name)
        host = webhttp.html_escape(entry["host"])
        port = webhttp.html_escape(entry["port"])
        overview = webhttp.html_escape(entry.get("overview", ""))
        overview_html = entry.get("overview_html")
        num_players = entry.get("num_players")
        max_players = entry.get("max_players")
        active = entry.get("active", False)
        owner_name = entry["owner"]
        owner = webhttp.html_escape(owner_name)
        owner_token = entry.get("owner_code") or owner_name
        owner_url = f"/users/{urllib.parse.quote(owner_token, safe='')}"
        screenshot_id = entry.get("screenshot_id")
        screenshot = None
        full_image = None
        if screenshot_id:
            urls = uploads.screenshot_urls(screenshot_id)
            screenshot = urls.get("thumb")
            full_image = urls.get("full") or screenshot
        actions_html = entry.get("actions_html") or ""
        actions_block = f"<div class=\"card-actions\">{actions_html}</div>" if actions_html else ""
        if overview_html:
            description_block = overview_html
        else:
            empty_overview = _ui_text("empty_states.overview")
            description_block = (
                f"<p>{overview}</p>" if overview else f"<p class=\"muted\">{webhttp.html_escape(empty_overview)}</p>"
            )
        screenshot_html = ""
        card_class = "card-top"
        if screenshot:
            card_class = "card-top"
            screenshot_html = (
                f"<img class=\"thumb\" src=\"{webhttp.html_escape(screenshot)}\" "
                f"data-full=\"{webhttp.html_escape(full_image)}\" alt=\"{webhttp.html_escape(_label('screenshot_alt'))}\">"
            )
        else:
            card_class = "card-top no-thumb"
        if num_players is not None and max_players is not None:
            online_value = f"{num_players} / {max_players}"
        elif num_players is not None:
            online_value = f"{num_players}"
        elif max_players is not None:
            online_value = f"— / {max_players}"
        else:
            online_value = "—"
        online_prefix = webhttp.html_escape(_status("online_prefix"))
        offline_label = webhttp.html_escape(_status("offline"))
        online_label = (
            f"<span class=\"online-label\">{online_prefix}</span> <span class=\"online-value\">{online_value}</span>"
            if active
            else f"<span class=\"online-label inactive\">{offline_label}</span>"
        )
        online_class = "online active" if active else "online inactive"
        endpoint_html = f"<div class=\"endpoint\">{host}:{port}</div>" if active else ""

        code = entry.get("code")
        if code:
            link_target = urllib.parse.quote(str(code), safe="")
        else:
            link_target = urllib.parse.quote(raw_name, safe="")
        name_link = f'<a class="server-link" href="/server/{link_target}">{name}</a>'
        card_id = entry.get("id")
        if card_id is None:
            card_id = raw_name
        cards.append(
            f"""<article class="card" data-server-id="{webhttp.html_escape(str(card_id))}">
  <header>
    <div class="title-block">
      <h3>{name_link}</h3>
      <div class="owner-line">{webhttp.html_escape(_label("owner_by"))} <a class="owner-link" href="{owner_url}">{owner}</a></div>
    </div>
    <div class="online-block">
      <div class="{online_class}">{online_label}</div>
      {endpoint_html}
      {actions_block}
    </div>
  </header>
  <div class="{card_class}">
    {screenshot_html}
    <div class="card-details">
      {description_block}
    </div>
  </div>
</article>"""
        )

    if not cards:
        empty_servers = _ui_text("empty_states.servers")
        cards_html = f"<p class=\"muted\">{webhttp.html_escape(empty_servers)}</p>"
    else:
        cards_html = "<div class=\"card-grid\">" + "".join(cards) + "</div>"

    return f"""{header_html}<div class="server-cards" data-role="server-cards">{cards_html}</div>
{lightbox.render_lightbox()}
{lightbox.render_lightbox_script()}"""


def render_server_section(
    rows,
    timeout,
    status,
    entry_builder,
    header_title=None,
    header_title_html=None,
    toggle_on_url=None,
    toggle_off_url=None,
    header_actions_html=None,
    csrf_token="",
    refresh_url=None,
    refresh_interval=10,
    allow_actions=False,
    refresh_animate=False,
    sort_entries=True,
):
    from karma.server_status import is_active

    status = (status or "all").lower()
    if status not in ("all", "active", "inactive"):
        status = "all"
    active_count = 0
    inactive_count = 0
    entries = []
    for row in rows:
        active = is_active(row, timeout)
        if active:
            active_count += 1
        else:
            inactive_count += 1
        if status == "active" and not active:
            continue
        if status == "inactive" and active:
            continue
        entry = entry_builder(row, active)
        if entry is None:
            continue
        if "active" not in entry:
            entry["active"] = active
        entries.append(entry)

    if sort_entries:
        entries.sort(key=lambda item: item.get("num_players") if item.get("num_players") is not None else -1, reverse=True)
    summary_text = config.format_text(_template("server_summary"), active=active_count, inactive=inactive_count)
    if status == "inactive":
        toggle_url = toggle_off_url
        toggle_label = _action("toggle_show_online")
    elif status == "active":
        toggle_url = toggle_on_url
        toggle_label = _action("toggle_show_offline")
    else:
        toggle_url = toggle_off_url
        toggle_label = _action("toggle_show_online")
    section_html = render_server_cards(
        entries,
        header_title=header_title,
        header_title_html=header_title_html,
        summary_text=summary_text,
        toggle_url=toggle_url,
        toggle_label=toggle_label,
        header_actions_html=header_actions_html,
    )
    if refresh_url:
        refresh_attr = webhttp.html_escape(refresh_url)
        allow_value = "1" if allow_actions else "0"
        inactive_value = "1" if status == "inactive" else "0"
        animate_value = "1" if refresh_animate else "0"
        csrf_attr = webhttp.html_escape(csrf_token or "")
        return (
            f'<section class="server-section" data-refresh-url="{refresh_attr}" '
            f'data-refresh-interval="{refresh_interval}" data-allow-actions="{allow_value}" '
            f'data-show-inactive="{inactive_value}" data-animate="{animate_value}" '
            f'data-csrf-token="{csrf_attr}">{section_html}</section>'
        )
    return section_html


def render_admins_section(
    admins,
    show_controls=False,
    show_add_form=False,
    admin_input="",
    form_prefix="/users",
    notice_html=None,
    header_title_html=None,
    csrf_token="",
):
    csrf_html = csrf_input(csrf_token)
    username_label = webhttp.html_escape(_label("username"))
    trust_label = webhttp.html_escape(_label("trust_admins"))
    actions_label = webhttp.html_escape(_label("actions"))
    yes_label = webhttp.html_escape(_status("yes"))
    no_label = webhttp.html_escape(_status("no"))
    add_admin_label = webhttp.html_escape(_label("add_admin_by_username"))
    add_admin_button = webhttp.html_escape(_action("add_admin"))
    if show_controls:
        remove_label = webhttp.html_escape(_action("remove"))
        admin_rows = "".join(
            f"""<tr>
  <td><a class="admin-user-link" href="/users/{urllib.parse.quote(admin.get("code") or admin["username"], safe='')}">{webhttp.html_escape(admin["username"])}</a></td>
  <td class="center-cell">
    <form method="post" action="{form_prefix}/admins/trust" class="js-toggle-form admin-toggle">
      {csrf_html}
      <input type="hidden" name="username" value="{webhttp.html_escape(admin["username"])}">
      <label class="switch">
        <input type="checkbox" name="trust_admins" value="1" {"checked" if admin["trust_admins"] else ""}>
        <span class="slider"></span>
      </label>
    </form>
  </td>
  <td class="center-cell">
    <form method="post" action="{form_prefix}/admins/remove" class="admin-action">
      {csrf_html}
      <input type="hidden" name="username" value="{webhttp.html_escape(admin["username"])}">
      <button type="submit" class="secondary small">{remove_label}</button>
    </form>
  </td>
</tr>"""
            for admin in admins
        ) or f"<tr><td colspan=\"3\">{webhttp.html_escape(_ui_text('empty_states.admins'))}</td></tr>"
        table_head = f"""<thead>
    <tr>
      <th>{username_label}</th>
      <th class="center-cell">{trust_label}</th>
      <th class="center-cell">{actions_label}</th>
    </tr>
  </thead>"""
    else:
        admin_rows = "".join(
            f"""<tr>
  <td><a class="admin-user-link" href="/users/{urllib.parse.quote(admin.get("code") or admin["username"], safe='')}">{webhttp.html_escape(admin["username"])}</a></td>
  <td>{yes_label if admin["trust_admins"] else no_label}</td>
</tr>"""
            for admin in admins
        ) or f"<tr><td colspan=\"2\">{webhttp.html_escape(_ui_text('empty_states.admins'))}</td></tr>"
        table_head = f"""<thead>
    <tr>
      <th>{username_label}</th>
      <th class="center-cell">{trust_label}</th>
    </tr>
  </thead>"""

    add_form_html = ""
    if show_add_form:
        safe_input = webhttp.html_escape(admin_input)
        add_form_html = f"""<form method="post" action="{form_prefix}/admins/add">
  {csrf_html}
  <div class="row">
    <div>
      <label for="admin_username">{add_admin_label}</label>
      <input id="admin_username" name="username" required value="{safe_input}">
    </div>
  </div>
  <div class="actions">
    <button type="submit">{add_admin_button}</button>
  </div>
</form>
"""

    notice_block = notice_html or ""
    title_html = header_title_html or webhttp.html_escape(_section("admins"))
    return f"""<h2>{title_html}</h2>
{notice_block}
<table>
  {table_head}
  <tbody>
    {admin_rows}
  </tbody>
</table>
{add_form_html}"""
