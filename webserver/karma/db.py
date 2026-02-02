import contextlib
import os
import secrets
import sqlite3


def default_db_path():
    data_dir = _resolve_data_dir()
    db_file = _resolve_db_file()
    return os.path.join(data_dir, db_file)


def _resolve_data_dir():
    from karma import config

    settings = config.get_config()
    community_dir = config.get_community_dir() or config.get_config_dir()
    data_dir = config.require_setting(settings, "database.database_directory")
    if os.path.isabs(data_dir):
        return data_dir
    return os.path.normpath(os.path.join(community_dir, data_dir))


def _resolve_db_file():
    from karma import config

    settings = config.get_config()
    db_file = config.require_setting(settings, "database.database_file")
    return str(db_file)


def init_db(db_path):
    os.makedirs(os.path.dirname(db_path), exist_ok=True)
    conn = sqlite3.connect(db_path)
    conn.execute("PRAGMA foreign_keys = ON")
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL COLLATE NOCASE UNIQUE,
            email TEXT NOT NULL COLLATE NOCASE UNIQUE,
            language TEXT,
            code TEXT,
            password_hash TEXT NOT NULL,
            password_salt TEXT NOT NULL,
            is_admin INTEGER NOT NULL DEFAULT 0,
            is_admin_manual INTEGER NOT NULL DEFAULT 0,
            is_locked INTEGER NOT NULL DEFAULT 0,
            locked_at TEXT,
            deleted INTEGER NOT NULL DEFAULT 0,
            deleted_at TEXT,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        )
        """
    )
    user_columns = [row[1] for row in conn.execute("PRAGMA table_info(users)").fetchall()]
    if "code" not in user_columns:
        conn.execute("ALTER TABLE users ADD COLUMN code TEXT")
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS servers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL COLLATE NOCASE UNIQUE,
            code TEXT,
            overview TEXT,
            description TEXT,
            host TEXT NOT NULL,
            port INTEGER NOT NULL CHECK(port >= 1 AND port <= 65535),
            max_players INTEGER CHECK(max_players IS NULL OR max_players >= 0),
            num_players INTEGER CHECK(num_players IS NULL OR num_players >= 0),
            owner_user_id INTEGER NOT NULL,
            last_heartbeat INTEGER,
            screenshot_id TEXT,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(owner_user_id) REFERENCES users(id) ON DELETE CASCADE,
            CHECK (max_players IS NULL OR num_players IS NULL OR num_players <= max_players)
        )
        """
    )
    columns = [row[1] for row in conn.execute("PRAGMA table_info(servers)").fetchall()]
    if "code" not in columns:
        conn.execute("ALTER TABLE servers ADD COLUMN code TEXT")
    conn.execute("CREATE UNIQUE INDEX IF NOT EXISTS servers_host_port_unique ON servers(host, port)")
    conn.execute("CREATE UNIQUE INDEX IF NOT EXISTS servers_code_unique ON servers(code)")
    _ensure_server_codes(conn)
    duplicates = conn.execute(
        """
        SELECT host, port, COUNT(*) AS total
          FROM servers
         GROUP BY host, port
        HAVING total > 1
        LIMIT 1
        """
    ).fetchone()
    if duplicates:
        conn.close()
        raise ValueError(
            f"[karma] Error: duplicate server host+port found in database ({duplicates[0]}:{duplicates[1]})."
        )
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS user_admins (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            owner_user_id INTEGER NOT NULL,
            admin_user_id INTEGER NOT NULL,
            trust_admins INTEGER NOT NULL DEFAULT 0,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(owner_user_id, admin_user_id),
            FOREIGN KEY(owner_user_id) REFERENCES users(id) ON DELETE CASCADE,
            FOREIGN KEY(admin_user_id) REFERENCES users(id) ON DELETE CASCADE
        )
        """
    )
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS password_resets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            token TEXT NOT NULL,
            expires_at INTEGER NOT NULL,
            FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
        )
        """
    )
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS rate_limits (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            bucket_key TEXT NOT NULL,
            created_at REAL NOT NULL
        )
        """
    )
    conn.execute("CREATE INDEX IF NOT EXISTS servers_owner_user_id_idx ON servers(owner_user_id)")
    conn.execute("CREATE INDEX IF NOT EXISTS servers_last_heartbeat_idx ON servers(last_heartbeat)")
    conn.execute("CREATE INDEX IF NOT EXISTS user_admins_owner_idx ON user_admins(owner_user_id)")
    conn.execute("CREATE INDEX IF NOT EXISTS user_admins_admin_idx ON user_admins(admin_user_id)")
    conn.execute("CREATE UNIQUE INDEX IF NOT EXISTS password_resets_token_unique ON password_resets(token)")
    conn.execute("CREATE INDEX IF NOT EXISTS password_resets_user_idx ON password_resets(user_id)")
    conn.execute("CREATE INDEX IF NOT EXISTS rate_limits_key_idx ON rate_limits(bucket_key)")
    conn.execute("CREATE INDEX IF NOT EXISTS rate_limits_created_at_idx ON rate_limits(created_at)")
    conn.execute("CREATE UNIQUE INDEX IF NOT EXISTS users_code_unique ON users(code)")
    _ensure_user_codes(conn)
    conn.close()


def _generate_server_code(length=6):
    alphabet = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
    return "".join(secrets.choice(alphabet) for _ in range(length))


def _unique_server_code(conn):
    while True:
        code = _generate_server_code()
        existing = conn.execute("SELECT 1 FROM servers WHERE code = ? LIMIT 1", (code,)).fetchone()
        if not existing:
            return code


def _ensure_server_codes(conn):
    rows = conn.execute("SELECT id FROM servers WHERE code IS NULL OR code = ''").fetchall()
    for row in rows:
        code = _unique_server_code(conn)
        conn.execute("UPDATE servers SET code = ? WHERE id = ?", (code, row[0]))
    if rows:
        conn.commit()


def _generate_user_code(length=6):
    alphabet = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
    return "".join(secrets.choice(alphabet) for _ in range(length))


def _unique_user_code(conn):
    while True:
        code = _generate_user_code()
        existing = conn.execute("SELECT 1 FROM users WHERE code = ? LIMIT 1", (code,)).fetchone()
        if not existing:
            return code


def _ensure_user_codes(conn):
    rows = conn.execute("SELECT id FROM users WHERE code IS NULL OR code = ''").fetchall()
    for row in rows:
        code = _unique_user_code(conn)
        conn.execute("UPDATE users SET code = ? WHERE id = ?", (code, row[0]))
    if rows:
        conn.commit()


def connect(db_path):
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys = ON")
    return conn


@contextlib.contextmanager
def connect_ctx(db_path=None):
    db_path = db_path or default_db_path()
    conn = connect(db_path)
    try:
        yield conn
    finally:
        conn.close()


def add_server(conn, record):
    while True:
        code = record.get("code") or _unique_server_code(conn)
        try:
            conn.execute(
                """
                INSERT INTO servers
                    (name, code, overview, description, host, port, max_players, num_players, owner_user_id,
                     screenshot_id, last_heartbeat)
                VALUES
                    (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    record.get("name"),
                    code,
                    record.get("overview"),
                    record.get("description"),
                    record["host"],
                    record["port"],
                    record.get("max_players"),
                    record.get("num_players"),
                    record.get("owner_user_id"),
                    record.get("screenshot_id"),
                    record.get("last_heartbeat"),
                ),
            )
            conn.commit()
            return
        except sqlite3.IntegrityError as exc:
            message = str(exc).lower()
            if "servers_code_unique" in message or "servers.code" in message:
                if record.get("code"):
                    raise
                continue
            raise


def update_server(conn, server_id, record):
    conn.execute(
        """
        UPDATE servers
        SET name = ?,
            overview = ?,
            description = ?,
            host = ?,
            port = ?,
            max_players = ?,
            num_players = ?,
            owner_user_id = ?,
            screenshot_id = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
        """,
        (
            record.get("name"),
            record.get("overview"),
            record.get("description"),
            record["host"],
            record["port"],
            record.get("max_players"),
            record.get("num_players"),
            record.get("owner_user_id"),
            record.get("screenshot_id"),
            server_id,
        ),
    )
    conn.commit()


def update_heartbeat(conn, server_id, timestamp, num_players=None, max_players=None):
    fields = ["last_heartbeat = ?", "updated_at = CURRENT_TIMESTAMP"]
    values = [timestamp]
    if num_players is not None:
        fields.insert(1, "num_players = ?")
        values.insert(1, num_players)
    if max_players is not None:
        fields.insert(1, "max_players = ?")
        values.insert(1, max_players)
    values.append(server_id)
    conn.execute(
        f"""
        UPDATE servers
        SET {", ".join(fields)}
        WHERE id = ?
        """,
        values,
    )
    conn.commit()


def update_server_port(conn, server_id, port):
    conn.execute(
        "UPDATE servers SET port = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
        (port, server_id),
    )
    conn.commit()


def delete_server(conn, server_id):
    conn.execute("DELETE FROM servers WHERE id = ?", (server_id,))
    conn.commit()


def list_servers(conn):
    return conn.execute(
        """
        SELECT servers.*,
               users.username AS owner_username,
               users.code AS owner_code
          FROM servers
          JOIN users ON users.id = servers.owner_user_id
         WHERE users.deleted = 0
         ORDER BY servers.created_at ASC
        """
    ).fetchall()


def get_server(conn, server_id):
    return conn.execute(
        """
        SELECT servers.*,
               users.username AS owner_username,
               users.code AS owner_code
          FROM servers
          JOIN users ON users.id = servers.owner_user_id
         WHERE servers.id = ?
        """,
        (server_id,),
    ).fetchone()


def get_server_by_name(conn, name):
    return conn.execute(
        """
        SELECT servers.*,
               users.username AS owner_username,
               users.code AS owner_code
          FROM servers
          JOIN users ON users.id = servers.owner_user_id
         WHERE servers.name = ?
           AND users.deleted = 0
        """,
        (name,),
    ).fetchone()


def get_server_by_code(conn, code):
    return conn.execute(
        """
        SELECT servers.*,
               users.username AS owner_username,
               users.code AS owner_code
          FROM servers
          JOIN users ON users.id = servers.owner_user_id
         WHERE servers.code = ?
           AND users.deleted = 0
        """,
        (code,),
    ).fetchone()


def get_server_by_host_port(conn, host, port):
    return conn.execute(
        """
        SELECT servers.*,
               users.username AS owner_username,
               users.code AS owner_code
          FROM servers
          JOIN users ON users.id = servers.owner_user_id
         WHERE servers.host = ?
           AND servers.port = ?
        """,
        (host, port),
    ).fetchone()


def list_ports_by_host(conn, host):
    rows = conn.execute("SELECT port FROM servers WHERE host = ? ORDER BY port", (host,)).fetchall()
    return [row["port"] for row in rows]


def list_user_servers(conn, user_id):
    return conn.execute(
        """
        SELECT servers.*,
               users.username AS owner_username,
               users.code AS owner_code
          FROM servers
          JOIN users ON users.id = servers.owner_user_id
         WHERE servers.owner_user_id = ?
           AND users.deleted = 0
         ORDER BY servers.created_at DESC
        """,
        (user_id,),
    ).fetchall()


def add_user(conn, username, email, password_hash, password_salt, is_admin=False, is_admin_manual=False, language=None, code=None):
    while True:
        user_code = code or _unique_user_code(conn)
        try:
            conn.execute(
                """
                INSERT INTO users (username, email, language, code, password_hash, password_salt, is_admin, is_admin_manual)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    username,
                    email,
                    language,
                    user_code,
                    password_hash,
                    password_salt,
                    1 if is_admin else 0,
                    1 if is_admin_manual else 0,
                ),
            )
            conn.commit()
            return
        except sqlite3.IntegrityError as exc:
            message = str(exc).lower()
            if "users_code_unique" in message or "users.code" in message:
                if code:
                    raise
                continue
            raise


def update_user_email(conn, user_id, email):
    conn.execute(
        "UPDATE users SET email = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
        (email, user_id),
    )
    conn.commit()


def update_user_username(conn, user_id, username):
    conn.execute(
        "UPDATE users SET username = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
        (username, user_id),
    )
    conn.commit()


def update_user_language(conn, user_id, language):
    conn.execute(
        "UPDATE users SET language = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
        (language, user_id),
    )
    conn.commit()


def set_user_admin(conn, user_id, enabled):
    conn.execute(
        "UPDATE users SET is_admin = ?, is_admin_manual = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
        (1 if enabled else 0, 1 if enabled else 0, user_id),
    )
    conn.commit()


def set_user_admin_manual(conn, user_id, enabled):
    conn.execute(
        "UPDATE users SET is_admin_manual = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
        (1 if enabled else 0, user_id),
    )
    conn.commit()


def recompute_admin_flags(conn, root_admin_id):
    users = conn.execute(
        "SELECT id, deleted FROM users"
    ).fetchall()
    admin_ids = set()
    if root_admin_id:
        admin_ids.add(root_admin_id)
        direct_admins = conn.execute(
            "SELECT admin_user_id, trust_admins FROM user_admins WHERE owner_user_id = ?",
            (root_admin_id,),
        ).fetchall()
        direct_ids = [row["admin_user_id"] for row in direct_admins]
        admin_ids.update(direct_ids)
        trusted_ids = [row["admin_user_id"] for row in direct_admins if row["trust_admins"]]
        for trusted_id in trusted_ids:
            rows = conn.execute(
                "SELECT admin_user_id FROM user_admins WHERE owner_user_id = ?",
                (trusted_id,),
            ).fetchall()
            admin_ids.update(row["admin_user_id"] for row in rows)
    for row in users:
        if row["deleted"]:
            conn.execute("UPDATE users SET is_admin = 0 WHERE id = ?", (row["id"],))
        else:
            conn.execute(
                "UPDATE users SET is_admin = ? WHERE id = ?",
                (1 if row["id"] in admin_ids else 0, row["id"]),
            )
    conn.commit()


def set_user_password(conn, user_id, password_hash, password_salt):
    conn.execute(
        "UPDATE users SET password_hash = ?, password_salt = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
        (password_hash, password_salt, user_id),
    )
    conn.commit()


def set_user_locked(conn, user_id, locked, locked_at=None):
    if locked:
        if locked_at:
            conn.execute(
                """
                UPDATE users
                SET is_locked = 1,
                    locked_at = ?,
                    updated_at = CURRENT_TIMESTAMP
                WHERE id = ?
                """,
                (locked_at, user_id),
            )
        else:
            conn.execute(
                """
                UPDATE users
                SET is_locked = 1,
                    locked_at = CURRENT_TIMESTAMP,
                    updated_at = CURRENT_TIMESTAMP
                WHERE id = ?
                """,
                (user_id,),
            )
    else:
        conn.execute(
            """
            UPDATE users
            SET is_locked = 0,
                locked_at = NULL,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
            """,
            (user_id,),
        )
    conn.commit()


def set_user_deleted(conn, user_id, deleted, deleted_at=None):
    if deleted:
        if deleted_at:
            conn.execute(
                """
                UPDATE users
                SET deleted = 1,
                    deleted_at = ?,
                    updated_at = CURRENT_TIMESTAMP
                WHERE id = ?
                """,
                (deleted_at, user_id),
            )
        else:
            conn.execute(
                """
                UPDATE users
                SET deleted = 1,
                    deleted_at = CURRENT_TIMESTAMP,
                    updated_at = CURRENT_TIMESTAMP
                WHERE id = ?
                """,
                (user_id,),
            )
    else:
        conn.execute(
            """
            UPDATE users
            SET deleted = 0,
                deleted_at = NULL,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
            """,
            (user_id,),
        )
    conn.commit()


def delete_user(conn, user_id):
    conn.execute("DELETE FROM user_admins WHERE owner_user_id = ? OR admin_user_id = ?", (user_id, user_id))
    conn.execute("DELETE FROM users WHERE id = ?", (user_id,))
    conn.commit()


def get_user_by_email(conn, email):
    return conn.execute("SELECT * FROM users WHERE email = ?", (email,)).fetchone()


def get_user_by_id(conn, user_id):
    return conn.execute("SELECT * FROM users WHERE id = ?", (user_id,)).fetchone()


def get_user_by_username(conn, username):
    return conn.execute("SELECT * FROM users WHERE username = ?", (username,)).fetchone()


def get_user_by_code(conn, code):
    return conn.execute("SELECT * FROM users WHERE code = ?", (code,)).fetchone()


def list_users(conn):
    return conn.execute("SELECT * FROM users ORDER BY created_at DESC").fetchall()


def list_user_admins(conn, owner_user_id):
    rows = conn.execute(
        """
        SELECT users.username,
               users.code,
               users.id AS admin_user_id,
               user_admins.trust_admins
        FROM user_admins
        JOIN users ON users.id = user_admins.admin_user_id
        WHERE user_admins.owner_user_id = ?
          AND users.deleted = 0
          AND users.is_locked = 0
        ORDER BY users.username
        """,
        (owner_user_id,),
    ).fetchall()
    return [
        {
            "username": row["username"],
            "code": row["code"],
            "admin_user_id": row["admin_user_id"],
            "trust_admins": bool(row["trust_admins"]),
        }
        for row in rows
    ]


def add_user_admin(conn, owner_user_id, admin_user_id, trust_admins=False):
    conn.execute(
        """
        INSERT OR IGNORE INTO user_admins (owner_user_id, admin_user_id, trust_admins)
        VALUES (?, ?, ?)
        """,
        (owner_user_id, admin_user_id, 1 if trust_admins else 0),
    )
    conn.commit()


def remove_user_admin(conn, owner_user_id, admin_user_id):
    conn.execute(
        "DELETE FROM user_admins WHERE owner_user_id = ? AND admin_user_id = ?",
        (owner_user_id, admin_user_id),
    )
    conn.commit()


def set_user_admin_trust(conn, owner_user_id, admin_user_id, trust_admins):
    conn.execute(
        """
        UPDATE user_admins
        SET trust_admins = ?
        WHERE owner_user_id = ? AND admin_user_id = ?
        """,
        (1 if trust_admins else 0, owner_user_id, admin_user_id),
    )
    conn.commit()


def add_password_reset(conn, user_id, token, expires_at):
    conn.execute(
        "INSERT INTO password_resets (user_id, token, expires_at) VALUES (?, ?, ?)",
        (user_id, token, expires_at),
    )
    conn.commit()


def get_password_reset(conn, token):
    return conn.execute(
        "SELECT * FROM password_resets WHERE token = ?",
        (token,),
    ).fetchone()


def delete_password_reset(conn, token):
    conn.execute("DELETE FROM password_resets WHERE token = ?", (token,))
    conn.commit()


def delete_expired_password_resets(conn, now_timestamp):
    conn.execute("DELETE FROM password_resets WHERE expires_at < ?", (int(now_timestamp),))
    conn.commit()
