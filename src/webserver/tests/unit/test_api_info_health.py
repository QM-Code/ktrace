import io
import json
import os
import tempfile
import unittest


def _load_modules():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    if root_dir not in os.sys.path:
        os.sys.path.insert(0, root_dir)
    from karma import config, db, webhttp
    from karma.handlers import api

    return config, db, webhttp, api


def _make_request(webhttp, method, path):
    environ = {
        "REQUEST_METHOD": method,
        "PATH_INFO": path,
        "QUERY_STRING": "",
        "wsgi.input": io.BytesIO(b""),
        "CONTENT_LENGTH": "0",
        "CONTENT_TYPE": "",
        "REMOTE_ADDR": "127.0.0.1",
    }
    return webhttp.Request(environ)


class ApiInfoHealthTest(unittest.TestCase):
    def setUp(self):
        self.config, self.db, self.webhttp, self.api = _load_modules()
        self.temp_dir = tempfile.TemporaryDirectory(prefix="karma-unit-community-")
        self.addCleanup(self.temp_dir.cleanup)

        community_config = {
            "server": {
                "community_name": "Unit Test Community",
                "community_description": "Unit test description",
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

    def _decode_json_response(self, response, expected_status):
        status, headers, body = response
        self.assertEqual(status, expected_status)
        self.assertTrue(any(k.lower() == "content-type" for k, _ in headers))
        self.assertIsInstance(body, bytes)
        return json.loads(body.decode("utf-8"))

    def test_health_get_returns_ok_true(self):
        request = _make_request(self.webhttp, "GET", "/api/health")

        payload = self._decode_json_response(self.api.handle(request), "200 OK")
        self.assertEqual(payload, {"ok": True})

    def test_health_non_get_is_rejected(self):
        request = _make_request(self.webhttp, "POST", "/api/health")

        payload = self._decode_json_response(self.api.handle(request), "405 Method Not Allowed")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "method_not_allowed")

    def test_info_get_returns_community_metadata(self):
        request = _make_request(self.webhttp, "GET", "/api/info")

        payload = self._decode_json_response(self.api.handle(request), "200 OK")
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["community_name"], "Unit Test Community")
        self.assertEqual(payload["community_description"], "Unit test description")

    def test_info_non_get_is_rejected(self):
        request = _make_request(self.webhttp, "POST", "/api/info")

        payload = self._decode_json_response(self.api.handle(request), "405 Method Not Allowed")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "method_not_allowed")

    def test_unknown_path_returns_not_found(self):
        request = _make_request(self.webhttp, "GET", "/api/nope")

        payload = self._decode_json_response(self.api.handle(request), "404 Not Found")
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"], "not_found")


if __name__ == "__main__":
    unittest.main()
