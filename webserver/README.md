# Community Web Server

Python WSGI webserver for the community list (accounts, submissions, admin tooling).

## Requirements

- Python 3.8+ (tested with 3.12). macOS/Linux: `python3`. Windows: `py -3` or `python`.
- Pillow (`python3-pil`) for screenshot resizing.
- Waitress (`pip install waitress`) for multi-request serving (falls back to wsgiref).

## Configuration model

There are **two** config layers:

- **Distribution config**: `webserver/config.json`  
  This is the authoritative source of defaults and must include required keys. End users should not edit it.

- **Community config**: `<community>/config.json`  
  Per-community overrides written by `bin/initialize.py`.

If a required key is missing from `webserver/config.json`, the server **errors on startup** with a clear message.

### Language / localization

- Default language is set by `webserver/config.json` → `server.language`.
- A community can override it with `<community>/config.json` → `server.language`.
- Translation files live under `webserver/strings/<lang>.json` (for example `webserver/strings/es.json`).
- Community overrides can be placed under `<community>/strings/<lang>.json` and will override the distribution strings (for matching keys).
- If a `language` is selected, its `strings/<lang>.json` must exist (distribution or community). Missing keys fall back to `strings/en.json`.
- Community `config.json` can also override individual string keys (e.g., `ui_text.*`) without replacing the full language file.
- Language selection order:
  1) User preference (Account → Personal settings)
  2) Browser `Accept-Language` header (if a matching `strings/<lang>.json` exists)
  3) Community config `server.language`
  4) Distribution config `server.language`

### Required keys in `webserver/config.json`

- `server.host`
- `server.port`
- `server.community_name`
- `server.admin_user`
- `server.session_secret`
- `database.database_directory`
- `database.database_file`
- `uploads.upload_directory`
- `uploads.max_request_bytes`
- `heartbeat.timeout_seconds`
- `server.language`
- `pages.servers.overview_max_chars`
- `pages.servers.auto_refresh`
- `pages.servers.auto_refresh_animate`
- `uploads.screenshots.limits.*` and `uploads.screenshots.{thumbnail,full}.*`
- `debug.*`
- `debug.disable_browser_language_detection`
- `debug.reset_rate_limits`
- `debug.pretty_print_json`
- `session_cookie.*`
- `security_headers.*`
- `cache_headers.static`
- `cache_headers.uploads`
- `logging.enabled`
- `logging.level`
- `logging.rotation`
- `logging.max_bytes`
- `logging.backups`
- `logging.utc`
- `logging.dir`
- `logging.server_file`
- `logging.include_user`
- `logging.paths`
- `httpserver.threads`
- `rate_limits.*`

Rate limiting is stored in the community SQLite database so limits work across processes. Set
`debug.reset_rate_limits` to `true` during development to clear all stored rate-limit entries.

### Logging

Logs are written under `<community>/<logging.dir>/`. `logging.server_file` defines the main
application log file. Request logs are controlled by `logging.paths`, which is a map of path
prefixes (case-sensitive) to logging rules (e.g. `logging.paths["/api/"]`). Paths are matched by prefix, with more specific
paths taking priority. If a matched entry has `override: true`, logging stops at that entry;
otherwise, logging continues to less specific matches. If a `file` is `null`, logging for
that prefix is disabled while still honoring `override`.

Set `logging.include_user` to include the logged-in username in request logs.

Rotation is controlled by `logging.rotation` (`daily` or `size`).

### Required keys in `webserver/strings/en.json`

- `meta.direction`
- `languages`
- `ui_text.*`
- `placeholders.*` (optional; missing placeholders are simply omitted)

### Community config fields

`bin/initialize.py` creates a minimal community config with (defaults come from `strings/<lang>.json` under `scripts.initialize.*`):

- `server.community_name`
- `server.port`
- `server.admin_user`
- `server.session_secret`

You can also add:

- `server.community_description`
- `server.language`

### Example: full `webserver/config.json`

```json
{
  "server": {
    "community_name": "My Community",
    "community_description": "",
    "admin_user": "Admin",
    "host": "0.0.0.0",
    "port": 8080,
    "language": "en",
    "session_secret": null,
    "trusted_proxies": []
  },
  "database": {
    "database_directory": "data",
    "database_file": "karma.db"
  },
  "uploads": {
    "upload_directory": "uploads",
    "max_request_bytes": 10485760,
    "screenshots": {
      "limits": {
        "min_bytes": 0,
        "max_bytes": 8388608,
        "min_width": 640,
        "min_height": 360,
        "max_width": 3840,
        "max_height": 2160
      },
      "thumbnail": {
        "width": 480,
        "height": 270
      },
      "full": {
        "width": 1600,
        "height": 900
      }
    }
  },
  "debug": {
    "heartbeat": false,
    "auth": false,
    "disable_browser_language_detection": false,
    "reset_rate_limits": false,
    "pretty_print_json": false
  },
  "pages": {
    "servers": {
      "overview_max_chars": 160,
      "auto_refresh": 10,
      "auto_refresh_animate": true
    }
  },
  "rate_limits": {
    "login": {
      "window_seconds": 60,
      "max_requests": 10
    },
    "register": {
      "window_seconds": 60,
      "max_requests": 5
    },
    "forgot": {
      "window_seconds": 60,
      "max_requests": 5
    },
    "reset": {
      "window_seconds": 60,
      "max_requests": 5
    },
    "api_auth": {
      "window_seconds": 60,
      "max_requests": 30
    },
    "api_user_registered": {
      "window_seconds": 60,
      "max_requests": 60
    }
  },
  "session_cookie": {
    "secure": false,
    "same_site": "Lax",
    "http_only": true
  },
  "security_headers": {
    "content_security_policy": "default-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'",
    "referrer_policy": "same-origin",
    "x_content_type_options": "nosniff"
  },
  "cache_headers": {
    "static": "public, max-age=86400",
    "uploads": "public, max-age=86400"
  },
  "logging": {
    "enabled": true,
    "level": "INFO",
    "rotation": "daily",
    "max_bytes": 10485760,
    "backups": 7,
    "utc": true,
    "dir": "logs",
    "server_file": "server.log",
    "include_user": false,
    "paths": {
      "/api/": {
        "file": "api.log",
        "override": true
      },
      "/api/heartbeat": {
        "file": null,
        "override": true
      },
      "/": {
        "file": "access.log"
      }
    }
  },
  "heartbeat": {
    "timeout_seconds": 120
  },
  "httpserver": {
    "threads": 8
  }
}
```

### Example: full `webserver/strings/en.json`

```json
{
  "languages": {
    "en": "English",
    "es": "Español",
    "fr": "French",
    "de": "German",
    "pt": "Portuguese",
    "ru": "Russian",
    "jp": "Japanese",
    "zh": "Chinese",
    "it": "Italian",
    "hi": "Hindi",
    "ar": "Arabic",
    "ko": "Korean",
  },
  "ui_text": {
    "hints": {
      "markdown_supported": "Markdown supported."
    },
    "counter": {
      "overview": "{count}/{max} characters"
    },
    "warnings": {
      "overview_over_limit": "Overview must be {max} characters or fewer."
    },
    "empty_states": {
      "overview": "No overview provided.",
      "description": "No description provided.",
      "community_description": "No description yet.",
      "servers": "No servers listed yet.",
      "admins": "No admins assigned yet.",
      "users": "No users yet."
    },
    "errors": {
      "method_not_allowed": {
        "title": "Method Not Allowed",
        "message": "This action is not allowed."
      },
      "forbidden": {
        "title": "Forbidden",
        "message": "You do not have permission to access this page."
      },
      "not_found": {
        "title": "Not Found",
        "message": "The requested page could not be found."
      },
      "missing_user": {
        "title": "Missing User",
        "message": "User is required."
      },
      "user_not_found": {
        "title": "User Not Found",
        "message": "User not found."
      },
      "missing_server": {
        "title": "Missing Server",
        "message": "Server name is required."
      },
      "server_not_found": {
        "title": "Server Not Found",
        "message": "Server not found."
      }
    },
    "messages": {
      "account": {
        "register_missing_fields": "Username, email, and password are required.",
        "register_username_spaces": "Username cannot contain spaces.",
        "register_username_reserved": "Username is reserved.",
        "register_username_taken": "Username '{username}' already taken.",
        "register_email_taken": "Email already registered.",
        "login_failed": "Login failed.",
        "account_locked": "Account locked.",
        "rate_limited": "Too many attempts. Please wait and try again.",
        "forgot_notice": "If the account exists, a reset link is ready.",
        "reset_link_prefix": "Reset link:",
        "reset_links_notice": "Reset links are displayed here until email delivery is configured.",
        "reset_invalid": "Reset link is invalid or expired.",
        "reset_password_required": "Password is required.",
        "reset_success": "Password updated. Please sign in."
      },
      "submit": {
        "upload_too_large": "Upload too large.",
        "missing_required": "Host, port, and server name are required.",
        "port_number": "Port must be a number.",
        "name_taken": "Server name is already taken.",
        "success_notice": "Thanks! Your server has been added. It will appear online after the next heartbeat."
      },
      "server_edit": {
        "upload_too_large": "Upload too large.",
        "missing_required": "Host, port, and server name are required.",
        "port_number": "Port must be a number.",
        "owner_not_found": "Owner username not found.",
        "name_taken": "Server name is already taken.",
        "host_port_taken": "Host/port {host}:{port} is already claimed by {name}."
      },
      "server_profile": {
        "heartbeat_never": "Never",
        "heartbeat_unknown": "Unknown"
      },
      "users": {
        "username_required": "Username is required.",
        "username_self_add": "You cannot add yourself.",
        "user_not_found": "User not found.",
        "username_email_required": "Username and email are required.",
        "username_spaces": "Username cannot contain spaces.",
        "username_reserved": "Username is reserved.",
        "root_username_locked": "Root admin username cannot be changed.",
        "username_taken": "Username already taken.",
        "email_in_use": "Email already in use.",
        "email_required": "Email is required.",
        "language_invalid": "Language selection is not available.",
        "create_missing_fields": "Username, email, and password are required.",
        "email_registered": "Email already registered."
      },
      "uploads": {
        "missing_pillow": "Image processing library (Pillow) is not installed.",
        "empty_file": "Uploaded file was empty.",
        "file_too_small": "File too small (min {min_bytes} bytes).",
        "file_too_large": "File too large (max {max_bytes} bytes).",
        "invalid_image": "Uploaded file is not a valid image.",
        "resolution_too_small": "Image resolution too small.",
        "resolution_too_large": "Image resolution too large."
      },
      "info": {
        "updated": "Community info updated."
      }
    },
    "nav": {
      "info": "Info",
      "servers": "Servers",
      "account": "Account",
      "users": "Users",
      "login": "Login",
      "logout": "Logout"
    },
    "header": {
      "signed_in_as": "Signed in as"
    },
    "confirm": {
      "title": "Confirm action",
      "message": "Are you sure?",
      "cancel_label": "Cancel",
      "ok_label": "Delete"
    },
    "confirmations": {
      "delete_server": "Delete this server permanently?",
      "delete_user": "Delete this user permanently?",
      "reinstate_user": "Reinstate this user?"
    },
    "status": {
      "online": "Online",
      "online_prefix": "Online:",
      "offline": "Offline",
      "yes": "Yes",
      "no": "No"
    },
    "labels": {
      "owner_by": "by",
      "username": "Username",
      "email": "Email",
      "email_or_username": "Email or Username",
      "password": "Password",
      "new_password": "New password",
      "new_password_optional": "New password (optional)",
      "reset_password_optional": "Reset password (optional)",
      "language": "Language",
      "language_auto": "Auto (browser preference)",
      "host": "Host",
      "port": "Port",
      "server_name": "Server Name",
      "overview": "Overview",
      "description": "Description",
      "owner_username_optional": "Owner username (optional)",
      "screenshot_upload": "World screenshot (PNG/JPG)",
      "screenshot_replace": "Replace screenshot (PNG/JPG)",
      "screenshot_alt": "Screenshot",
      "screenshot_preview_alt": "Screenshot preview",
      "lightbox_close": "Close",
      "community_name": "Community name",
      "community_description": "Community description",
      "add_admin_by_username": "Add admin by username",
      "admin": "Admin",
      "lock": "Lock",
      "deleted": "Deleted",
      "status": "Status",
      "owner": "Owner",
      "players": "Players",
      "last_heartbeat": "Last heartbeat",
      "admins": "Admins",
      "trust_admins": "Trust user's admins",
      "actions": "Actions"
    },
    "actions": {
      "create_account": "Create account",
      "sign_in": "Sign In",
      "generate_reset_link": "Generate reset link",
      "update_password": "Update password",
      "save": "Save",
      "cancel": "Cancel",
      "submit_for_approval": "Submit for Approval",
      "return_to_servers": "Return to servers",
      "submit_another": "Submit another server",
      "save_changes": "Save changes",
      "edit": "Edit",
      "delete": "Delete",
      "delete_user": "Delete user",
      "reinstate": "Reinstate",
      "reinstate_user": "Reinstate user",
      "remove": "Remove",
      "add_admin": "Add admin",
      "add_server": "Add server",
      "create_user": "Create User",
      "personal_settings": "Personal settings",
      "edit_info": "Edit info",
      "register_here": "Register here",
      "already_have_account": "Already have an account?",
      "need_account": "Need an account?",
      "forgot_password": "Forgot password?",
      "back_to_login": "Back to login",
      "toggle_show_offline": "Show offline servers",
      "toggle_show_online": "Show online servers",
      "view_server": "View server"
    },
    "titles": {
      "create_account": "Create account",
      "login": "Login",
      "reset_password": "Reset password",
      "set_new_password": "Set a new password",
      "submit_server": "Submit Server",
      "server_added": "Server added",
      "edit_server": "Edit server",
      "users": "Users",
      "edit_user": "Edit User",
      "personal_settings": "Personal settings",
      "community_info": "Community Info",
      "admin_docs": "Admin Documentation",
      "server_profile": "Server profile",
      "server_list": "Server List",
      "user_profile": "User profile"
    },
    "sections": {
      "servers": "Servers",
      "admins": "Admins",
      "add_user": "Add User"
    },
    "templates": {
      "server_summary": "<strong>{active} online</strong> / {inactive} offline"
    },
    "admin_docs": {
      "html": "<p>You are a <strong>Community Admin</strong>.</p><p>Community admins can:</p><ol><li>Administer the community website (this site).</li><li>Act as admins in all game worlds.</li></ol><p>Admin propagation is one level deep. The root admin assigns primary admins. If a primary admin is marked as trusted, their direct admins become secondary admins.</p><p>Admin abilities like modifying users, locking accounts, and editing worlds should only apply to admins below you in the hierarchy. This enforcement is still being expanded.</p>",
      "diagram_alt": "Admin flow diagram"
    },
    "admin_notices": {
      "root_html": "<div class=\"admin-notice\"><div>You are logged in as the <span class=\"admin-notice-strong\">Root Admin</span>.</div><ul><li>Any admins you create will also be granted Community Admin status.</li><li class=\"admin-notice-warning\">If you enable \"Trust User's Admins\", all of that user's admins will also become Community Admins.</li><li>Please read the <a href=\"/admin-docs\">Admin Documentation</a> and proceed with caution.</li></ul></div>",
      "trusted_html": "<div class=\"admin-notice\"><div>You are a <span class=\"admin-notice-strong\">Trusted Primary Community Admin</span>.</div><ul><li>Any admins you create will also be granted Community Admin status.</li><li>Please read the <a href=\"/admin-docs\">Admin Documentation</a> and proceed with caution.</li></ul></div>"
    }
  },
  "placeholders": {
    "add_server": {
      "host": "example.com or 10.0.0.5",
      "port": "11899",
      "name": "Required unique name",
      "overview": "Short summary for listings",
      "description": "Optional description"
    },
    "users": {
      "reset_password": "Leave blank to keep current password",
      "new_password": "Leave blank to keep current password"
    }
  }
}
```

### Example: minimal community `config.json` (created by `bin/initialize.py`)

```json
{
  "server": {
    "community_name": "My Community",
    "port": 8080,
    "admin_user": "Admin",
    "session_secret": "CHANGE_ME"
  }
}
```

## Directory layout

- `karma/`: Python package.
- `bin/`: CLI tools.
- `config.json`: distribution defaults (authoritative).
- `strings/`: distribution strings (one file per language, e.g. `strings/en.json`).
- `communities/`: per-community storage roots.
  - `communities/<name>/config.json`: per-community overrides.
  - `communities/<name>/strings/`: per-community string overrides (one file per language).

Community directories are **user-writable** and contain:
- `config.json`
- `<database.database_directory>/` (SQLite DB)
- `<uploads.upload_directory>/` (screenshots)

## Getting started

1) Initialize a community directory (must be empty). This writes `config.json`, creates the database directory + uploads directory, and seeds the admin user:

```
python3 ./bin/initialize.py /path/to/communities/example1
```

2) Start the server:

```
python3 ./bin/start.py ./communities/example1
```

Windows (PowerShell):

```
py -3 .\bin\start.py .\communities\example1
```

You can override the port at startup:

```
python3 ./bin/start.py ./communities/example1 -p 8081
```

## Data import/export

Restore a fresh database from JSON:

```
python3 ./bin/db-restore.py ./communities/example1 -f /path/to/data.json
```

Merge JSON into an existing database:

```
python3 ./bin/db-merge.py ./communities/example1 -f /path/to/data.json
```

Snapshot the database:

```
python3 ./bin/db-snapshot.py ./communities/example1 /path/to/export.json
```

If no output path is provided, `db-snapshot.py` writes to:
`communities/<name>/<database.database_directory>/snapshot-<timestamp>.json`. Use `-z` to write a zip.

## JSON format

Import/export JSON matches `communities/*/<database.database_directory>/test-data.json` and includes:

- Users:
  - `username`, `code`, `email`
  - `password` (plaintext, import only) **or** `password_hash` + `password_salt`
  - `is_admin`
  - `is_locked`, `locked_at`, `deleted`, `deleted_at`
  - `admin_list` (array of usernames or `{ "username", "trust_admins" }`)
- Servers:
  - `name`, `code`, `overview`, `description`, `host`, `port`, `owner`, `owner_code`
  - `screenshot_id`
  - `max_players`, `num_players`, `last_heartbeat` omitted (heartbeat populates them)

## Scripts

- `bin/initialize.py`: prompts for community defaults + admin credentials, writes community config, seeds DB.
- `bin/start.py`: starts the WSGI server (`-p <port>` optional).
- `bin/db-restore.py`: restore DB from JSON (`-f <json-file>` required, fails if DB exists).
- `bin/db-merge.py`: merge JSON into existing DB (`-f <json-file>` required).
- `bin/db-snapshot.py`: export DB to JSON (`-z` to zip).
- `tests/makedata.py`: generate random users/servers (`-s <server-num>`, `-u <user-num>`).
- `tests/heartbeat.py`: repeatedly heartbeat about half of servers (dev helper).
- `tests/validate_strings.py`: validate that language files include all keys from `strings/en.json`.
- `bin/clean-images.py`: delete unreferenced screenshots (`--dry-run` supported).

## Admin user behavior

- `server.admin_user` is the **root admin name** for admin propagation.
- Changing `server.admin_user` does **not** grant admin rights; the DB flag controls that.
- Admin propagation updates when admin flags are recomputed.

## URLs

- `/` → `/servers`
- `/servers` HTML list (active + inactive)
- `/servers/active` HTML list (active only)
- `/servers/inactive` HTML list (inactive only)
- `/server/<server>` server profile (accepts server code or server name)
- `/api/servers` JSON list (overview only; includes active + inactive)
- `/api/servers/active` JSON list (overview only; active servers)
- `/api/servers/inactive` JSON list (overview only; inactive servers)
- `/api/server/<code|name|id>` JSON for a single server (overview + full description)
- `/api/users/<code|name>` JSON for user + server list
- `/servers/add` submit a server (login required)
- `/server/<server>/edit` edit server (owner/admin)
- `/server/delete?id=<id>` delete server (owner/admin)
- `/users` list users (admin only)
- `/users/<code|username>` user profile
- `/users/<username>/edit` personal settings
- `/register`, `/login`, `/logout`, `/forgot`, `/reset` auth
- `/api/admins` admin list for a host+port
- `/api/heartbeat` game server heartbeat
- `/api/auth` game server auth (POST only unless `debug.auth` is true)
- `/api/info` community info
- `/api/user_registered` check username
- `/api/health` health check

## Notes

- Markdown is rendered on `/server/<server>` (description) and `/info` (community description) with sanitization.
- Placeholders are configurable under `placeholders.*` in `webserver/config.json`.
- `server.host` may be `0.0.0.0` for binding, but clients should use a reachable address.
