import io
import json
import os
import tempfile
import unittest
from unittest import mock


def _load_modules():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    if root_dir not in os.sys.path:
        os.sys.path.insert(0, root_dir)
    from karma import config, db, router, webhttp

    return config, db, router, webhttp


def _make_request(webhttp, method, path, accept_language="en-US,en;q=0.9"):
    environ = {
        "REQUEST_METHOD": method,
        "PATH_INFO": path,
        "QUERY_STRING": "",
        "wsgi.input": io.BytesIO(b""),
        "CONTENT_LENGTH": "0",
        "CONTENT_TYPE": "",
        "REMOTE_ADDR": "127.0.0.1",
        "HTTP_ACCEPT_LANGUAGE": accept_language,
    }
    return webhttp.Request(environ)


class RouterDispatchTest(unittest.TestCase):
    def setUp(self):
        self.config, self.db, self.router, self.webhttp = _load_modules()
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

    def test_dispatch_root_redirects_to_active_servers(self):
        request = _make_request(self.webhttp, "GET", "/")
        status, headers, body = self.router.dispatch(request)

        self.assertEqual(status, "302 Found")
        self.assertIn(("Location", "/servers/active"), headers)
        self.assertEqual(body, b"")

    def test_dispatch_api_servers_routes_status_all(self):
        request = _make_request(self.webhttp, "GET", "/api/servers")
        with mock.patch("karma.router.api_servers.handle", return_value=("200 OK", [], b"{}")) as handler:
            result = self.router.dispatch(request)

        self.assertEqual(result[0], "200 OK")
        handler.assert_called_once_with(request, status="all")

    def test_dispatch_api_servers_routes_status_active(self):
        request = _make_request(self.webhttp, "GET", "/api/servers/active")
        with mock.patch("karma.router.api_servers.handle", return_value=("200 OK", [], b"{}")) as handler:
            result = self.router.dispatch(request)

        self.assertEqual(result[0], "200 OK")
        handler.assert_called_once_with(request, status="active")

    def test_dispatch_api_servers_routes_status_inactive(self):
        request = _make_request(self.webhttp, "GET", "/api/servers/inactive")
        with mock.patch("karma.router.api_servers.handle", return_value=("200 OK", [], b"{}")) as handler:
            result = self.router.dispatch(request)

        self.assertEqual(result[0], "200 OK")
        handler.assert_called_once_with(request, status="inactive")

    def test_dispatch_api_server_path_sets_token(self):
        request = _make_request(self.webhttp, "GET", "/api/server/alpha%20name")
        with mock.patch("karma.router.api_server.handle", return_value=("200 OK", [], b"{}")) as handler:
            result = self.router.dispatch(request)

        self.assertEqual(result[0], "200 OK")
        self.assertEqual(request.query["token"], ["alpha name"])
        handler.assert_called_once_with(request)

    def test_dispatch_api_user_path_sets_token(self):
        request = _make_request(self.webhttp, "GET", "/api/users/owner%2Fa")
        with mock.patch("karma.router.api_user.handle", return_value=("200 OK", [], b"{}")) as handler:
            result = self.router.dispatch(request)

        self.assertEqual(result[0], "200 OK")
        self.assertEqual(request.query["token"], ["owner/a"])
        handler.assert_called_once_with(request)

    def test_dispatch_api_info_forwards_to_api_handler(self):
        request = _make_request(self.webhttp, "GET", "/api/info")
        with mock.patch("karma.router.api.handle", return_value=("200 OK", [], b"{}")) as handler:
            result = self.router.dispatch(request)

        self.assertEqual(result[0], "200 OK")
        handler.assert_called_once_with(request)

    def test_dispatch_static_traversal_is_rejected(self):
        request = _make_request(self.webhttp, "GET", "/static/../secret.txt")
        result = self.router.dispatch(request)

        self.assertIsNone(result)

    def test_dispatch_upload_traversal_is_rejected(self):
        request = _make_request(self.webhttp, "GET", "/uploads/../secret.txt")
        result = self.router.dispatch(request)

        self.assertIsNone(result)

    def test_dispatch_static_file_served_with_cache_header(self):
        request = _make_request(self.webhttp, "GET", "/static/style.css")
        status, headers, body_iter = self.router.dispatch(request)

        self.assertEqual(status, "200 OK")
        self.assertIn(("Content-Type", "text/css; charset=utf-8"), headers)
        self.assertTrue(any(key == "Cache-Control" for key, _ in headers))
        body = b"".join(body_iter)
        self.assertIn(b"body", body)

    def test_dispatch_upload_file_served_with_cache_header(self):
        uploads_dir = os.path.join(self.temp_dir.name, "uploads")
        os.makedirs(uploads_dir, exist_ok=True)
        upload_path = os.path.join(uploads_dir, "test.png")
        with open(upload_path, "wb") as handle:
            handle.write(b"\x89PNG\r\n\x1a\nPNGDATA")

        request = _make_request(self.webhttp, "GET", "/uploads/test.png")
        status, headers, body_iter = self.router.dispatch(request)

        self.assertEqual(status, "200 OK")
        self.assertIn(("Content-Type", "image/png"), headers)
        self.assertTrue(any(key == "Cache-Control" for key, _ in headers))
        body = b"".join(body_iter)
        self.assertEqual(body, b"\x89PNG\r\n\x1a\nPNGDATA")


if __name__ == "__main__":
    unittest.main()
