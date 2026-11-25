"""
SVG Grease Pencil Lottie — import SVG as Grease Pencil and export animated Grease Pencil to Lottie.

Copyright (C) 2026 multlabs (crantisz@gmail.com, to@multlabs.com)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

Transcode arbitrary Unicode names to Latin letters (Lottie ``nm``, etc.).
"""


import unicodedata

# Russian (informal / ISO-like), Ukrainian, Belarusian, Serbian/Macedonian extras.
# Longer ``src`` strings must be tried before shorter prefixes (e.g. ``шч`` before ``ш``).
_RAW_MULTI: tuple[tuple[str, str], ...] = (
    ("шч", "shch"),  # rare digraph in some texts
    ("Шч", "Shch"),
    ("щ", "shch"),
    ("Щ", "Shch"),
    ("ж", "zh"),
    ("Ж", "Zh"),
    ("х", "kh"),
    ("Х", "Kh"),
    ("ц", "ts"),
    ("Ц", "Ts"),
    ("ч", "ch"),
    ("Ч", "Ch"),
    ("ш", "sh"),
    ("Ш", "Sh"),
    ("ю", "yu"),
    ("Ю", "Yu"),
    ("я", "ya"),
    ("Я", "Ya"),
    ("й", "y"),
    ("Й", "Y"),
    ("ё", "yo"),
    ("Ё", "Yo"),
    ("ъ", ""),  # hard sign: omit in names
    ("Ъ", ""),
    ("ь", ""),  # soft sign
    ("Ь", ""),
    ("ы", "y"),
    ("Ы", "Y"),
    ("э", "e"),
    ("Э", "E"),
    ("ї", "yi"),
    ("Ї", "Yi"),
    ("є", "ye"),
    ("Є", "Ye"),
    ("і", "i"),
    ("І", "I"),
    ("ґ", "g"),
    ("Ґ", "G"),
    ("ў", "u"),
    ("Ў", "U"),
    ("љ", "lj"),
    ("Љ", "Lj"),
    ("њ", "nj"),
    ("Њ", "Nj"),
    ("ђ", "dj"),
    ("Ђ", "Dj"),
    ("ћ", "ts"),
    ("Ћ", "Ts"),
    ("џ", "dz"),
    ("Џ", "Dz"),
    ("ј", "j"),
    ("Ј", "J"),
    ("ќ", "kj"),
    ("Ќ", "Kj"),
    ("ѓ", "g"),
    ("Ѓ", "G"),
)

_MULTI: tuple[tuple[str, str], ...] = tuple(
    sorted(_RAW_MULTI, key=lambda p: len(p[0]), reverse=True)
)

_SINGLE: dict[str, str] = {
    "а": "a",
    "А": "A",
    "б": "b",
    "Б": "B",
    "в": "v",
    "В": "V",
    "г": "g",
    "Г": "G",
    "д": "d",
    "Д": "D",
    "е": "e",
    "Е": "E",
    "з": "z",
    "З": "Z",
    "и": "i",
    "И": "I",
    "к": "k",
    "К": "K",
    "л": "l",
    "Л": "L",
    "м": "m",
    "М": "M",
    "н": "n",
    "Н": "N",
    "о": "o",
    "О": "O",
    "п": "p",
    "П": "P",
    "р": "r",
    "Р": "R",
    "с": "s",
    "С": "S",
    "т": "t",
    "Т": "T",
    "у": "u",
    "У": "U",
    "ф": "f",
    "Ф": "F",
}

# Greek → Latin (informal single-letter; polytonic vowels decompose via NFKD later).
_GREEK_MAP: dict[str, str] = {
    "α": "a",
    "β": "b",
    "γ": "g",
    "δ": "d",
    "ε": "e",
    "ζ": "z",
    "η": "i",
    "θ": "th",
    "ι": "i",
    "κ": "k",
    "λ": "l",
    "μ": "m",
    "ν": "n",
    "ξ": "x",
    "ο": "o",
    "π": "p",
    "ρ": "r",
    "σ": "s",
    "ς": "s",
    "τ": "t",
    "υ": "y",
    "φ": "f",
    "χ": "ch",
    "ψ": "ps",
    "ω": "o",
    "Α": "A",
    "Β": "B",
    "Γ": "G",
    "Δ": "D",
    "Ε": "E",
    "Ζ": "Z",
    "Η": "I",
    "Θ": "Th",
    "Ι": "I",
    "Κ": "K",
    "Λ": "L",
    "Μ": "M",
    "Ν": "N",
    "Ξ": "X",
    "Ο": "O",
    "Π": "P",
    "Ρ": "R",
    "Σ": "S",
    "Τ": "T",
    "Υ": "Y",
    "Φ": "F",
    "Χ": "Ch",
    "Ψ": "Ps",
    "Ω": "O",
}


def _strip_combining_marks(s: str) -> str:
    return "".join(
        c
        for c in unicodedata.normalize("NFKD", s)
        if unicodedata.category(c) != "Mn"
    )


def to_latin_name(s: str | None, *, fallback: str = "unnamed") -> str:
    """
    Convert a Blender/Lottie display name to Latin letters and ASCII punctuation.

    Preserves spaces, digits, and ``._-``. Other non-ASCII symbols become ``_``.
    """
    if s is None:
        return fallback
    # NFC keeps composed Cyrillic letters (e.g. ``й``) so we do not strip them
    # into base ``и`` before the transliteration tables run.
    t = unicodedata.normalize("NFC", str(s).strip())
    if not t:
        return fallback

    # Longest multi-char sequences first (already ordered roughly by length)
    out: list[str] = []
    i = 0
    n = len(t)
    while i < n:
        matched = False
        for src, dst in _MULTI:
            if t.startswith(src, i):
                out.append(dst)
                i += len(src)
                matched = True
                break
        if matched:
            continue
        ch = t[i]
        rep = _SINGLE.get(ch)
        if rep is not None:
            out.append(rep)
            i += 1
            continue
        g = _GREEK_MAP.get(ch)
        if g is not None:
            out.append(g)
            i += 1
            continue
        out.append(ch)
        i += 1

    merged = "".join(out)
    merged = _strip_combining_marks(merged)
    merged = "".join(_GREEK_MAP.get(ch, ch) for ch in merged)

    safe: list[str] = []
    for ch in merged:
        o = ord(ch)
        if ch in " ._-":
            safe.append(ch)
        elif ch.isascii() and ch.isalnum():
            safe.append(ch)
        elif ch.isascii() and ch in "()[]{}+,=@&":
            safe.append(ch)
        elif o < 128:
            safe.append(ch)
        else:
            safe.append("_")

    result = "".join(safe).strip()
    while "__" in result:
        result = result.replace("__", "_")
    return result or fallback
