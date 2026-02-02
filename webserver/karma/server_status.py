import time


def is_active(row, timeout_seconds):
    last_seen = row["last_heartbeat"]
    if last_seen is None:
        return False
    return (int(time.time()) - int(last_seen)) <= int(timeout_seconds)
