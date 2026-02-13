import io
import json
import os
import tempfile
import time
import unittest
import urllib.parse


def _load_modules():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    if root_dir not in os.sys.path:
        os.sys.path.insert(0, root_dir)
    from karma import config, db, webhttp
    from karma.handlers import api
    return config, db, webhttp, api


def _make_post_request(webhttp, path, form_data, remote_addr="127.0.0.1"):
    body = urllib.parse.urlencode(form_data).encode("utf-8")
    environ = {
        "REQUEST_METHOD": "POST",
        "PATH_INFO": path,
        "QUERY_STRING": "",
        "wsgi.input": io.BytesIO(body),
        "CONTENT_LENGTH": str(len(body)),
        "CONTENT_TYPE": "application/x-www-form-urlencoded",
        "REMOTE_ADDR": remote_addr,
    }
    return webhttp.Request(environ)


class ApiHeartbeatTest(unittest.TestCase):
    def setUp(self):
        self.config, self.db, self.webhttp, self.api = _load_modules()
        self.temp_dir = tempfile.TemporaryDirectory(prefix="karma-unit-community-")
        self.addCleanup(self.temp_dir.cleanup)

        community_config = {
            "server": {
                "community_name": "Unit Test Community",
                "admin_user": "Admin",
                "session_secret": "unit-test-secret",
                "language": "en",
            }
        }
        config_path = os.path.join(self.temp_dir.name, "config.json")
        with open(config_path, "w", encoding="utf-8") as handle:
            json.dump(community_config, handle)
            handle.write("\n")

        self.config.set_community_dir(self.temp_dir.name)
        self.db.init_db(self.db.default_db_path())
        self._seed_data()

    def _seed_data(self):
        with self.db.connect_ctx() as conn:
            self.db.add_user(conn, "owner_a", "owner_a@example.local", "hash-a", "salt-a")
            owner = self.db.get_user_by_username(conn, "owner_a")
            self.assertIsNotNone(owner)
            self.db.add_server(
                conn,
                {
                    "name": "alpha",
                    "overview": "alpha overview",
                    "description": "alpha description",
                    "host": "127.0.0.1",
                    "port": 11901,
                    "owner_user_id": owner["id"],
                    "screenshot_id": None,
                    "max_players": 20,
                    "num_players": 0,
                    "last_heartbeat": None,
                },
            )
            self.db.add_server(
                conn,
                {
                    "name": "beta",
                    "overview": "beta overview",
                    "description": "beta description",
                    "host": "127.0.0.1",
                    "port": 11902,
                    "owner_user_id": owner["id"],
                    "screenshot_id": None,
                    "max_players": 20,
                    "num_players": 0,
                    "last_heartbeat": None,
                },
            )

    def _decode_json_response(self, response, expected_status):
        status, headers, body = response
        self.assertEqual(status, expected_status)
        self.assertTrue(any(k.lower() == "content-type" for k, _ in headers))
        self.assertIsInstance(body, bytes)
        return json.loads(body.decode("utf-8"))

    def test_post_heartbeat_updates_registered_server(self):
        request = _make_post_request(
            self.webhttp,
            "/api/heartbeat",
            {
                "server": "127.0.0.1:11901",
                "players": "4",
                "max": "20",
            },
            remote_addr="127.0.0.1",
        )

        payload = self._decode_json_response(self.api.handle(request), "200 OK")
        self.assertTrue(payload["ok"])

        with self.db.connect_ctx() as conn:
            server = self.db.get_server_by_host_port(conn, "127.0.0.1", 11901)
            self.assertIsNotNone(server)
            self.assertEqual(server["num_players"], 4)
            self.assertEqual(server["max_players"], 20)
            self.assertIsNotNone(server["last_heartbeat"])
            self.assertLessEqual(int(time.time()) - int(server["last_heartbeat"]), 5)

    def test_post_heartbeat_rejects_port_mismatch_for_registered_host(self):
        request = _make_post_request(
            self.webhttp,
            "/api/heartbeat",
            {
                "server": "127.0.0.1:65000",
                "players": "2",
                "max": "20",
            },
            remote_addr="127.0.0.1",
        )

        payload = self._decode_json_response(self.api.handle(request), "404 Not Found")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "port_mismatch")
        self.assertIn("there is no game registered on port 65000", payload.get("message", ""))

        with self.db.connect_ctx() as conn:
            server = self.db.get_server_by_host_port(conn, "127.0.0.1", 11901)
            self.assertIsNotNone(server)
            self.assertIsNone(server["last_heartbeat"])

    def test_rejects_non_get_post_method(self):
        environ = {
            "REQUEST_METHOD": "PUT",
            "PATH_INFO": "/api/heartbeat",
            "QUERY_STRING": "",
            "wsgi.input": io.BytesIO(b""),
            "CONTENT_LENGTH": "0",
            "CONTENT_TYPE": "",
            "REMOTE_ADDR": "127.0.0.1",
        }
        request = self.webhttp.Request(environ)

        payload = self._decode_json_response(self.api.handle(request), "405 Method Not Allowed")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "method_not_allowed")

    def test_get_rejected_when_debug_heartbeat_disabled(self):
        environ = {
            "REQUEST_METHOD": "GET",
            "PATH_INFO": "/api/heartbeat",
            "QUERY_STRING": "server=127.0.0.1:11901",
            "wsgi.input": io.BytesIO(b""),
            "CONTENT_LENGTH": "0",
            "CONTENT_TYPE": "",
            "REMOTE_ADDR": "127.0.0.1",
        }
        request = self.webhttp.Request(environ)

        payload = self._decode_json_response(self.api.handle(request), "403 Forbidden")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "heartbeat_debug_disabled")

    def test_rejects_missing_server(self):
        request = _make_post_request(self.webhttp, "/api/heartbeat", {}, remote_addr="127.0.0.1")

        payload = self._decode_json_response(self.api.handle(request), "400 Bad Request")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "missing_server")

    def test_rejects_invalid_port(self):
        request = _make_post_request(
            self.webhttp,
            "/api/heartbeat",
            {"server": "127.0.0.1:not-a-port"},
            remote_addr="127.0.0.1",
        )

        payload = self._decode_json_response(self.api.handle(request), "400 Bad Request")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "invalid_port")

    def test_rejects_invalid_players(self):
        request = _make_post_request(
            self.webhttp,
            "/api/heartbeat",
            {"server": "127.0.0.1:11901", "players": "-1"},
            remote_addr="127.0.0.1",
        )

        payload = self._decode_json_response(self.api.handle(request), "400 Bad Request")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "invalid_players")

    def test_rejects_invalid_max(self):
        request = _make_post_request(
            self.webhttp,
            "/api/heartbeat",
            {"server": "127.0.0.1:11901", "players": "1", "max": "bad"},
            remote_addr="127.0.0.1",
        )

        payload = self._decode_json_response(self.api.handle(request), "400 Bad Request")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "invalid_max")

    def test_rejects_players_greater_than_max(self):
        request = _make_post_request(
            self.webhttp,
            "/api/heartbeat",
            {"server": "127.0.0.1:11901", "players": "5", "max": "2"},
            remote_addr="127.0.0.1",
        )

        payload = self._decode_json_response(self.api.handle(request), "400 Bad Request")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "invalid_players")

    def test_rejects_invalid_newport(self):
        request = _make_post_request(
            self.webhttp,
            "/api/heartbeat",
            {"server": "127.0.0.1:11901", "newport": "70000"},
            remote_addr="127.0.0.1",
        )

        payload = self._decode_json_response(self.api.handle(request), "400 Bad Request")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "invalid_newport")

    def test_returns_host_not_found_for_unregistered_host(self):
        request = _make_post_request(
            self.webhttp,
            "/api/heartbeat",
            {"server": "127.0.0.2:11901"},
            remote_addr="127.0.0.2",
        )

        payload = self._decode_json_response(self.api.handle(request), "404 Not Found")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "host_not_found")

    def test_rejects_newport_when_target_port_already_in_use(self):
        request = _make_post_request(
            self.webhttp,
            "/api/heartbeat",
            {"server": "127.0.0.1:11901", "newport": "11902"},
            remote_addr="127.0.0.1",
        )

        payload = self._decode_json_response(self.api.handle(request), "409 Conflict")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "port_in_use")

    def test_accepts_newport_when_target_port_available(self):
        request = _make_post_request(
            self.webhttp,
            "/api/heartbeat",
            {"server": "127.0.0.1:11901", "newport": "11903", "players": "6", "max": "24"},
            remote_addr="127.0.0.1",
        )

        payload = self._decode_json_response(self.api.handle(request), "200 OK")
        self.assertTrue(payload["ok"])

        with self.db.connect_ctx() as conn:
            moved = self.db.get_server_by_host_port(conn, "127.0.0.1", 11903)
            self.assertIsNotNone(moved)
            self.assertEqual(moved["num_players"], 6)
            self.assertEqual(moved["max_players"], 24)


if __name__ == "__main__":
    unittest.main()
