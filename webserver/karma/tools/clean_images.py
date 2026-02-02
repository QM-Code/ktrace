import argparse
import os
import re

from karma import db, uploads


FILENAME_RE = re.compile(r"^([0-9a-f]{24})_(original|full|thumb)\.jpg$")


def _list_referenced_ids(conn):
    rows = conn.execute("SELECT screenshot_id FROM servers WHERE screenshot_id IS NOT NULL").fetchall()
    return {row["screenshot_id"] for row in rows if row["screenshot_id"]}


def clean_images(dry_run=False):
    uploads_dir = uploads._uploads_dir()
    if not os.path.isdir(uploads_dir):
        return 0, 0

    with db.connect_ctx() as conn:
        referenced = _list_referenced_ids(conn)

    removed = 0
    kept = 0
    for name in os.listdir(uploads_dir):
        match = FILENAME_RE.match(name)
        if not match:
            continue
        image_id = match.group(1)
        if image_id in referenced:
            kept += 1
            continue
        path = os.path.join(uploads_dir, name)
        if not dry_run:
            os.remove(path)
        removed += 1
    return removed, kept


def main():
    parser = argparse.ArgumentParser(description="Delete unreferenced screenshots in uploads.")
    parser.add_argument("--dry-run", action="store_true", help="Show what would be deleted without removing files.")
    args = parser.parse_args()
    removed, kept = clean_images(dry_run=args.dry_run)
    action = "Would remove" if args.dry_run else "Removed"
    print(f"{action} {removed} images; kept {kept}.")


if __name__ == "__main__":
    main()
