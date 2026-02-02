import getpass
import hashlib
import os


def main():
    password = getpass.getpass("Admin password: ")
    if not password:
        raise SystemExit("Password is required.")
    salt = os.urandom(16).hex()
    digest = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt.encode("utf-8"), 100_000).hex()
    print("admin_password_salt:", salt)
    print("admin_password_hash:", digest)


if __name__ == "__main__":
    main()
