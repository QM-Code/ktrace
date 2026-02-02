import threading
import time

from karma import config, db, webhttp


_LOCK = threading.Lock()


def allow(conn, key, max_requests, window_seconds):
    now = time.time()
    cutoff = now - window_seconds
    with _LOCK:
        conn.execute(
            "DELETE FROM rate_limits WHERE bucket_key = ? AND created_at < ?",
            (key, cutoff),
        )
        row = conn.execute(
            "SELECT COUNT(*) AS total FROM rate_limits WHERE bucket_key = ?",
            (key,),
        ).fetchone()
        total = row["total"] if row else 0
        if total >= max_requests:
            conn.commit()
            return False
        conn.execute(
            "INSERT INTO rate_limits (bucket_key, created_at) VALUES (?, ?)",
            (key, now),
        )
        conn.commit()
    return True


def check(settings, request, action):
    limit_cfg = config.require_setting(settings, f"rate_limits.{action}")
    max_requests = config.require_setting(limit_cfg, "max_requests", f"config.json rate_limits.{action}")
    window_seconds = config.require_setting(limit_cfg, "window_seconds", f"config.json rate_limits.{action}")
    reset_limits = bool(config.require_setting(settings, "debug.reset_rate_limits"))
    try:
        max_requests = int(max_requests)
        window_seconds = int(window_seconds)
    except (TypeError, ValueError):
        raise ValueError(f"[karma] Error: rate_limits.{action} must include integer max_requests and window_seconds.")
    client_ip = webhttp.client_ip(request.environ)
    key = f"{action}:{client_ip}"
    with db.connect_ctx() as conn:
        if reset_limits:
            with _LOCK:
                conn.execute("DELETE FROM rate_limits")
                conn.commit()
            return True
        return allow(conn, key, max_requests, window_seconds)
