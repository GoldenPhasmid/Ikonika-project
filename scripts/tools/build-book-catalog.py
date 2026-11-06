import argparse
import csv
import hashlib
import html
import json
import posixpath
import re
import sys
import zipfile
from pathlib import Path

BOOK_ID = re.compile(r"^[a-z0-9-]+$")
BOOK_LEVELS = {"beginner", "intermediate", "advanced"}
READABLE_SUFFIXES = {".epub", ".fb2", ".txt", ".pdf"}
TEXT_SUFFIXES = {".xhtml", ".html", ".htm", ".opf", ".ncx", ".css", ".xml", ".txt"}

GUTENBERG = re.compile(r"gutenberg", re.IGNORECASE)
BOILERPLATE_START = re.compile(r"<(header|footer|section|div)\b[^>]*\bid=\"pg-(?:header|footer)\"[^>]*>")
GUTENBERG_CSS_RULE = re.compile(r"[^{}]*gutenberg[^{}]*\{[^{}]*\}", re.IGNORECASE)
GUTENBERG_PARAGRAPH = re.compile(r"<(p|table|h[1-6])\b[^>]*>(?:(?!</\1>).)*?gutenberg(?:(?!</\1>).)*?</\1>", re.DOTALL | re.IGNORECASE)
PRINTED_LINE_NUMBER = re.compile(r"\s*<span\b[^>]*\bclass=\"(?:[^\"]*\s)?linenum(?:\s[^\"]*)?\"[^>]*>[^<]*</span>")
GENERATED_COVER = re.compile(r"\d+_\d+-cover\.(?:png|jpe?g)$")
TRANSCRIBER_NOTE = re.compile(r"<div\b[^>]*\bclass=\"[^\"]*\b(?:transnote|tnote)\b[^\"]*\"[^>]*>.*?</div>\s*", re.DOTALL | re.IGNORECASE)
TRANSCRIBER_LINK = re.compile(r"<p\b[^>]*>(?:(?!</p>).)*?>\s*Nota de transcripci[^<]*</a>\s*</p>\s*", re.DOTALL | re.IGNORECASE)
PLAIN_TEXT_MARKUP = re.compile(r"\.txt\.(?:xhtml|html)$")
PLAIN_ITALIC = re.compile(r"_([^_]+?)_")
CHAPTER_PARAGRAPH = re.compile(r"<p\b([^>]*)>([^<]{1,120})</p>")
CHAPTER_TITLE = re.compile(r"^(?:(?:CHAPITRE|CHAPTER)\s+)?[IVXLC]{1,7}\s*[.—–-]\s*\S.{0,80}$", re.DOTALL)
TXT_START = re.compile(r"^\*\*\* ?START OF (?:THE|THIS) PROJECT GUTENBERG.*$", re.MULTILINE | re.IGNORECASE)
TXT_END = re.compile(r"^\*\*\* ?END OF (?:THE|THIS) PROJECT GUTENBERG.*$", re.MULTILINE | re.IGNORECASE)


def sha256_of(path):
    digest = hashlib.sha256()

    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1 << 20), b""):
            digest.update(chunk)

    return digest.hexdigest()


def read_manifest(path):
    with path.open(encoding="utf-8", newline="") as file:
        rows = list(csv.DictReader(file, delimiter="\t"))

    missing = {"id", "title", "author", "level", "path"} - set(rows[0].keys() if rows else [])

    if missing:
        sys.exit(f"{path} has no column {', '.join(sorted(missing))}")

    return rows


def remove_boilerplate(markup):
    while (start := BOILERPLATE_START.search(markup)) is not None:
        tag = start.group(1)
        boundary = re.compile(r"<(/?)" + tag + r"\b[^>]*?(/?)>")
        depth = 1
        position = start.end()

        while depth > 0:
            found = boundary.search(markup, position)

            if found is None:
                sys.exit(f"An unclosed <{tag}> holds the Project Gutenberg header or footer")

            if found.group(1):
                depth -= 1
            elif not found.group(2):
                depth += 1

            position = found.end()

        markup = markup[:start.start()] + markup[position:]

    return markup


def italicise_underscores(markup):
    parts = re.split(r"(<[^>]+>)", markup)
    body_started = False

    for index, part in enumerate(parts):
        if part.startswith("<"):
            body_started = body_started or part.startswith("<body")

            continue

        if body_started and "_" in part:
            parts[index] = PLAIN_ITALIC.sub(r"<i>\1</i>", part).replace("_", "")

    return "".join(parts)


def promote_chapters(entries, spine_names, nav_name, ncx_name):
    chapters = []

    for name in spine_names:
        markup = entries[name].decode("utf-8")

        def promote(match):
            attributes, text = match.group(1), match.group(2)

            if not CHAPTER_TITLE.match(" ".join(html.unescape(text).split())):
                return match.group(0)

            anchor = re.search(r"\bid=\"([^\"]+)\"", attributes)

            if anchor is None:
                attributes = f" id=\"chapter-{len(chapters) + 1}\"{attributes}"
                anchor = re.search(r"\bid=\"([^\"]+)\"", attributes)

            chapters.append((name, anchor.group(1), " ".join(text.split())))

            return f"<h2{attributes}>{text}</h2>"

        entries[name] = CHAPTER_PARAGRAPH.sub(promote, markup).encode("utf-8")

    if len(chapters) < 3:
        return False

    def link(from_name, name, anchor):
        return posixpath.relpath(name, posixpath.dirname(from_name)) + "#" + anchor

    if nav_name is not None:
        items = "".join(f"\n        <li><a href=\"{link(nav_name, name, anchor)}\">{text}</a></li>" for name, anchor, text in chapters)
        nav = entries[nav_name].decode("utf-8")
        nav = re.sub(r"(<nav\b[^>]*>.*?<ol\b[^>]*>).*?(</ol>)", lambda match: match.group(1) + items + "\n      " + match.group(2), nav, count=1, flags=re.DOTALL)
        entries[nav_name] = nav.encode("utf-8")

    if ncx_name is not None:
        points = "".join(f"\n    <navPoint id=\"chapter-{number}\" playOrder=\"{number}\"><navLabel><text>{text}</text></navLabel>"
                         f"<content src=\"{link(ncx_name, name, anchor)}\"/></navPoint>"
                         for number, (name, anchor, text) in enumerate(chapters, start=1))
        ncx = entries[ncx_name].decode("utf-8")
        ncx = re.sub(r"(<navMap>).*?(</navMap>)", lambda match: match.group(1) + points + "\n  " + match.group(2), ncx, count=1, flags=re.DOTALL)
        entries[ncx_name] = ncx.encode("utf-8")

    return True


def visible_text(markup):
    body = re.search(r"<body\b[^>]*>(.*)</body>", markup, re.DOTALL)

    return re.sub(r"<[^>]+>", "", body.group(1) if body else markup).strip()


def drop_list_entries(markup, pattern, dropped_names):
    entry = re.compile(pattern, re.DOTALL)

    def keep(match):
        content = match.group(0)
        names = re.findall(r"(?:href|src)=\"([^\"#]*)", content)

        if GUTENBERG.search(content) or any(posixpath.basename(name) in dropped_names for name in names):
            return ""

        return content

    previous = None

    while previous != markup:
        previous = markup
        markup = entry.sub(keep, markup)

    return markup


def strip_epub(source, destination, book_id):
    with zipfile.ZipFile(source) as archive:
        entries = {info.filename: archive.read(info.filename) for info in archive.infolist()}

    container = entries["META-INF/container.xml"].decode("utf-8")
    opf_name = re.search(r"full-path=\"([^\"]+)\"", container).group(1)
    opf_directory = posixpath.dirname(opf_name)
    opf = entries[opf_name].decode("utf-8")
    title = re.search(r"<dc:title\b[^>]*>([^<]*)</dc:title>", opf).group(1)

    def entry_name(href):
        return posixpath.normpath(posixpath.join(opf_directory, href))

    manifest = {match.group(2): (match.group(1), match.group(0))
                for match in re.finditer(r"<item\b(?=[^>]*\bid=\"([^\"]+)\")(?=[^>]*\bhref=\"([^\"]+)\")[^>]*/>", opf)}
    dropped = set()

    for href, (item_id, item) in manifest.items():
        name = entry_name(href)

        if "application/xhtml+xml" not in item:
            continue

        markup = entries[name].decode("utf-8")
        markup = remove_boilerplate(markup)
        markup = TRANSCRIBER_NOTE.sub("", markup)
        markup = TRANSCRIBER_LINK.sub("", markup)
        markup = GUTENBERG_PARAGRAPH.sub("", markup)
        markup = PRINTED_LINE_NUMBER.sub("", markup)
        markup = re.sub(r"<title>[^<]*gutenberg[^<]*</title>", lambda match: f"<title>{title}</title>", markup, flags=re.IGNORECASE)

        if PLAIN_TEXT_MARKUP.search(name):
            markup = italicise_underscores(markup)
        entries[name] = markup.encode("utf-8")

        if not visible_text(markup) and "<image" not in markup and "<img" not in markup:
            dropped.add(href)

    cover_meta = re.search(r"<meta\b[^>]*\bname=\"cover\"[^>]*\bcontent=\"([^\"]+)\"", opf)
    cover_meta_id = cover_meta.group(1) if cover_meta is not None else None

    for href, (item_id, item) in manifest.items():
        if ("cover-image" in item or item_id == cover_meta_id) and GENERATED_COVER.search(href):
            dropped.add(href)

            for wrapper_href, (wrapper_id, wrapper_item) in manifest.items():
                if "application/xhtml+xml" in wrapper_item and posixpath.basename(href) in entries[entry_name(wrapper_href)].decode("utf-8"):
                    dropped.add(wrapper_href)

            opf = re.sub(r"<meta\b[^>]*name=\"cover\"[^>]*/>\s*", "", opf)

    for href in dropped:
        item_id, item = manifest[href]
        opf = opf.replace(item, "")
        opf = re.sub(r"<itemref\b[^>]*idref=\"" + re.escape(item_id) + r"\"[^>]*/>\s*", "", opf)
        entries.pop(entry_name(href), None)

    opf = re.sub(r"(<dc:identifier\b[^>]*>)[^<]*(</dc:identifier>)", r"\g<1>urn:ikonika:book:" + book_id + r"\g<2>", opf)
    opf = re.sub(r"<(dc:source|dc:publisher|meta)\b[^>]*>[^<]*gutenberg[^<]*</\1>\s*", "", opf, flags=re.IGNORECASE)
    opf = re.sub(r"\s*<!--[^>]*-->", "", opf)
    entries[opf_name] = opf.encode("utf-8")

    dropped_names = {posixpath.basename(href) for href in dropped}

    for name, data in list(entries.items()):
        if name.endswith(".css"):
            entries[name] = GUTENBERG_CSS_RULE.sub("", data.decode("utf-8")).encode("utf-8")

            continue

        if not name.endswith((".xhtml", ".html", ".ncx")):
            continue

        markup = data.decode("utf-8")
        markup = re.sub(r"<meta\b[^>]*name=\"dtb:uid\"[^>]*/>", f"<meta name=\"dtb:uid\" content=\"urn:ikonika:book:{book_id}\"/>", markup)
        markup = re.sub(r"\s*<meta\b[^>]*gutenberg[^>]*/>", "", markup, flags=re.IGNORECASE)
        markup = drop_list_entries(markup, r"<li\b[^>]*>(?:(?!<li\b).)*?</li>", dropped_names)
        markup = drop_list_entries(markup, r"<navPoint\b[^>]*>(?:(?!<navPoint\b).)*?</navPoint>", dropped_names)
        entries[name] = markup.encode("utf-8")

    spine_names = [entry_name(href) for item_id in re.findall(r"<itemref\b[^>]*idref=\"([^\"]+)\"", opf)
                   for href, (manifest_id, item) in manifest.items() if manifest_id == item_id and href not in dropped]
    nav_name = next((entry_name(href) for href, (item_id, item) in manifest.items() if "properties=\"nav\"" in item), None)
    ncx_name = next((entry_name(href) for href, (item_id, item) in manifest.items() if "application/x-dtbncx+xml" in item), None)
    toc_size = len(re.findall(r"<navPoint\b", entries[ncx_name].decode("utf-8"))) if ncx_name else 0

    if toc_size < 3 and promote_chapters(entries, spine_names, nav_name, ncx_name):
        print(f"{source.name}: rebuilt the contents from its chapter titles")

    leftovers = [name for name, data in entries.items()
                 if Path(name).suffix.lower() in TEXT_SUFFIXES and GUTENBERG.search(data.decode("utf-8", "replace"))]

    if leftovers:
        sys.exit(f"{source} still mentions Project Gutenberg in {', '.join(leftovers)}")

    with zipfile.ZipFile(destination, "w") as archive:
        archive.writestr(zipfile.ZipInfo("mimetype"), entries.pop("mimetype"), compress_type=zipfile.ZIP_STORED)

        for name, data in entries.items():
            archive.writestr(name, data, compress_type=zipfile.ZIP_DEFLATED)


def strip_txt(source, destination):
    text = source.read_text(encoding="utf-8")
    start = TXT_START.search(text)
    end = TXT_END.search(text)

    if start is None or end is None:
        sys.exit(f"{source} has no Project Gutenberg start and end lines")

    body = text[start.end():end.start()].strip()

    if GUTENBERG.search(body):
        sys.exit(f"{source} still mentions Project Gutenberg between its start and end lines")

    destination.write_text(body + "\n", encoding="utf-8")


def publish_book(source, destination, book_id):
    suffix = source.suffix.lower()

    if suffix == ".epub":
        strip_epub(source, destination, book_id)
    elif suffix == ".txt":
        strip_txt(source, destination)
    else:
        destination.write_bytes(source.read_bytes())


def main():
    parser = argparse.ArgumentParser(description="Builds a language's book catalog and its book assets.")
    parser.add_argument("manifest", type=Path, help="TSV with id, title, author, level and path columns")
    parser.add_argument("--target", required=True, help="language code the books are written in")
    parser.add_argument("--base-url", required=True, help="address the book files are published under")
    parser.add_argument("--out", type=Path, required=True, help="directory for the catalog and the book files")
    arguments = parser.parse_args()

    arguments.out.mkdir(parents=True, exist_ok=True)
    base_url = arguments.base_url.rstrip("/")
    books = []

    for row in read_manifest(arguments.manifest):
        source = (arguments.manifest.parent / row["path"]).resolve()

        if not BOOK_ID.match(row["id"]):
            sys.exit(f"Book id {row['id']!r} is not lowercase letters, digits and dashes")

        if row["level"] not in BOOK_LEVELS:
            sys.exit(f"Book {row['id']!r} needs a beginner, intermediate or advanced level")

        if source.suffix.lower() not in READABLE_SUFFIXES:
            sys.exit(f"{source} is not a format the reader opens")

        file_name = row["id"] + source.suffix.lower()
        asset = arguments.out / file_name
        publish_book(source, asset, row["id"])

        books.append({
            "id": row["id"],
            "title": row["title"],
            "author": row["author"],
            "level": row["level"],
            "file": file_name,
            "url": f"{base_url}/{file_name}",
            "bytes": asset.stat().st_size,
            "sha256": sha256_of(asset),
        })

    catalog = {"schema": 1, "target": arguments.target, "books": books}
    catalog_path = arguments.out / f"books-{arguments.target}.json"
    catalog_path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    print(f"{catalog_path}: {len(books)} books")


if __name__ == "__main__":
    main()
