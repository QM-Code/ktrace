#!/usr/bin/env python3
import os
import sys


def main():
    args = sys.argv[1:]
    if not args:
        raise SystemExit("usage: clean-images.py <community-directory> [--dry-run]")
    directory = args.pop(0)
    dry_run = False
    while args:
        token = args.pop(0)
        if token == "--dry-run":
            dry_run = True
        else:
            raise SystemExit("usage: clean-images.py <community-directory> [--dry-run]")

    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    if root_dir not in sys.path:
        sys.path.insert(0, root_dir)

    from karma import cli
    from karma.tools import clean_images as tool

    cli.bootstrap(directory, "usage: clean-images.py <community-directory> [--dry-run]")

    removed, kept = tool.clean_images(dry_run=dry_run)
    action = "Would remove" if dry_run else "Removed"
    print(f"{action} {removed} images; kept {kept}.")


if __name__ == "__main__":
    main()
