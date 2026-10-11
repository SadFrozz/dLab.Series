"""Prepare a tagged release from its exact changelog section and two installers."""
import argparse
import hashlib
import os
from pathlib import Path
import re


def extract_notes(tag, changelog):
    if not re.fullmatch(r"v\d+\.\d+(?:\.\d+)?", tag):
        raise ValueError("Expected a release tag such as v1.0 or v1.0.1")
    sections = list(re.finditer(r"^## (.+)$", changelog, re.MULTILINE))
    for index, section in enumerate(sections):
        title = section.group(1).strip()
        if title.split(maxsplit=1)[0] != tag:
            continue
        end = sections[index + 1].start() if index + 1 < len(sections) else len(changelog)
        body = changelog[section.end():end].strip()
        if not body:
            raise ValueError(f"Empty changelog section for {tag}")
        return title, body + "\n"
    raise ValueError(f"No changelog section for {tag}; refusing to publish")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tag")
    parser.add_argument("--changelog", type=Path, default=Path("CHANGELOG.md"))
    parser.add_argument("--assets", type=Path, required=True)
    args = parser.parse_args()
    title, notes = extract_notes(args.tag, args.changelog.read_text(encoding="utf-8"))
    names = ("dLab.Series-Windows-x64-Setup.exe", "dLab.Series-macOS-Universal.pkg")
    if {p.name for p in args.assets.iterdir()} - {"SHA256SUMS.txt"} != set(names):
        raise ValueError("Release requires exactly the Windows EXE and macOS PKG installers")
    checksums = []
    for name in names:
        path = args.assets / name
        if not path.is_file() or path.stat().st_size == 0:
            raise ValueError(f"Missing or empty release asset: {name}")
        with path.open("rb") as source:
            digest = hashlib.file_digest(source, "sha256").hexdigest()
        checksums.append(f"{digest}  {name}\n")
    (args.assets / "SHA256SUMS.txt").write_text("".join(checksums), encoding="utf-8")
    Path("release-notes.md").write_text(notes, encoding="utf-8")
    if output := os.environ.get("GITHUB_OUTPUT"):
        with open(output, "a", encoding="utf-8") as stream:
            stream.write(f"title={title}\n")
    print(f"Ready to publish {title}")


if __name__ == "__main__":
    main()
