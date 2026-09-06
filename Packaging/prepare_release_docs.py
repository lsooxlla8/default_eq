#!/usr/bin/env python3

"""Create the minimal documentation shipped beside release binaries."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


REPOSITORY = "https://github.com/lsooxlla8/default_eq"


def without_build_section(markdown: str) -> str:
    lines = markdown.splitlines(keepends=True)
    output: list[str] = []
    skipping = False

    for line in lines:
        if line.rstrip() == "## Build":
            skipping = True
            continue
        if skipping and line.startswith("## "):
            skipping = False
        if not skipping:
            output.append(line)

    return "".join(output).rstrip() + "\n"


def make_links_release_safe(markdown: str, revision: str) -> str:
    image_pattern = re.compile(r"(!\[[^]]*\]\()([^)]+)(\))")
    link_pattern = re.compile(r"((?<!\!)\[[^]]+\]\()([^)]+)(\))")

    def image_replacement(match: re.Match[str]) -> str:
        target = match.group(2)
        if target.startswith(("http://", "https://")):
            return match.group(0)
        return (
            match.group(1)
            + f"https://raw.githubusercontent.com/lsooxlla8/default_eq/{revision}/{target}"
            + match.group(3)
        )

    def link_replacement(match: re.Match[str]) -> str:
        target = match.group(2)
        if target.startswith(("http://", "https://", "#")):
            return match.group(0)
        return match.group(1) + f"{REPOSITORY}/blob/{revision}/{target}" + match.group(3)

    return link_pattern.sub(link_replacement, image_pattern.sub(image_replacement, markdown))


def use_consolidated_legal_references(markdown: str) -> str:
    source_notice = (
        "Exact repositories, revisions, licences, modifications, and code boundaries\n"
        "are documented in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md). Included\n"
        "license texts are in [`LICENSES/`](LICENSES/)."
    )
    package_notice = (
        "Exact repositories, revisions, licences, modifications, code boundaries,\n"
        "and complete licence texts are included in `LEGAL.txt`."
    )
    source_licence_links = (
        "See [`LICENSE.md`](LICENSE.md), [`LICENSES/`](LICENSES/), and\n"
        "[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)."
    )
    return markdown.replace(source_notice, package_notice).replace(
        source_licence_links, "See `LEGAL.txt` included with this package."
    )


def build_legal_text(root: Path, revision: str) -> str:
    sections = (
        ("PROJECT LICENSING", root / "LICENSE.md"),
        ("THIRD-PARTY NOTICES", root / "THIRD_PARTY_NOTICES.md"),
        ("GNU AFFERO GENERAL PUBLIC LICENSE VERSION 3", root / "LICENSES/AGPL-3.0.txt"),
        ("GNU GENERAL PUBLIC LICENSE VERSION 3", root / "LICENSES/GPL-3.0.txt"),
        ("SIL OPEN FONT LICENSE 1.1 - JETBRAINS MONO", root / "LICENSES/OFL-JetBrainsMono.txt"),
    )
    chunks = [
        "default_eq legal notices and licence texts\n",
        f"Corresponding source for this release: {REPOSITORY}/tree/{revision}\n",
    ]
    for title, path in sections:
        content = path.read_text(encoding="utf-8")
        if title == "PROJECT LICENSING":
            content = content.replace(
                "Except where a source file or `THIRD_PARTY_NOTICES.md` says otherwise,",
                "Except where the THIRD-PARTY NOTICES section below says otherwise,",
            ).replace(
                "The complete text is in\n`LICENSES/AGPL-3.0.txt`.",
                "The complete AGPL text appears below.",
            ).replace(
                "exact code\nboundaries, copyright notices, repositories, and revisions are in\n"
                "`THIRD_PARTY_NOTICES.md`. The complete GPL text is in\n"
                "`LICENSES/GPL-3.0.txt`.",
                "exact code boundaries, copyright notices, repositories, and revisions appear\n"
                "in the THIRD-PARTY NOTICES section below. The complete GPL text follows it.",
            )
        elif title == "THIRD-PARTY NOTICES":
            content = content.replace(
                "The complete license is\nincluded in `LICENSES/OFL-JetBrainsMono.txt`.",
                "The complete license appears below.",
            )
        chunks.extend(("\n" + "=" * 78 + "\n", title + "\n", "=" * 78 + "\n",
                       content))
    return "".join(chunks).rstrip() + "\n"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--revision", required=True)
    args = parser.parse_args()

    root = args.root.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)

    readme = without_build_section((root / "README.md").read_text(encoding="utf-8"))
    readme = use_consolidated_legal_references(readme)
    readme = make_links_release_safe(readme, args.revision)
    if "## Build" in readme:
        raise RuntimeError("release README still contains the Build section")

    (output / "README.md").write_text(readme, encoding="utf-8")
    (output / "LEGAL.txt").write_text(build_legal_text(root, args.revision), encoding="utf-8")


if __name__ == "__main__":
    main()
