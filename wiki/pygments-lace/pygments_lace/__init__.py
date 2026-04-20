"""Pygments lexer for the Lace probe scripting language."""

from pygments.lexer import RegexLexer, bygroups, words
from pygments.token import (
    Comment, Keyword, Name, Number, Operator, Punctuation, String, Text,
)


class LaceLexer(RegexLexer):
    name = "Lace"
    aliases = ["lace"]
    filenames = ["*.lace"]

    tokens = {
        "root": [
            # Comments
            (r"//.*$", Comment.Single),

            # Strings (double-quoted)
            (r'"', String.Double, "string"),

            # HTTP methods
            (words((
                "get", "post", "put", "patch", "delete",
            ), suffix=r"\b"), Keyword.Declaration),

            # Chain methods
            (r"\.(expect|check|assert|store|wait)\b", Keyword),

            # Helper functions
            (words(("json", "form", "schema"), suffix=r"\b"), Name.Builtin),

            # Operators
            (words((
                "eq", "neq", "lt", "lte", "gt", "gte",
                "and", "or", "not",
            ), suffix=r"\b"), Operator.Word),

            # Boolean and null literals
            (words(("true", "false", "null"), suffix=r"\b"), Keyword.Constant),

            # Built-in references
            (words(("this", "prev"), suffix=r"\b"), Name.Builtin.Pseudo),

            # Config and scope keywords
            (words((
                "headers", "body", "cookies", "cookieJar", "clearCookies",
                "redirects", "security", "timeout",
                "follow", "max", "rejectInvalidCerts", "ms", "action", "retries",
                "status", "bodySize", "totalDelayMs", "dns", "connect", "tls",
                "ttfb", "transfer", "size",
                "value", "op", "match", "mode", "options", "condition",
                "expect", "check",
            ), suffix=r"\b"), Name.Attribute),

            # Run-scope variables ($$var)
            (r"\$\$[a-zA-Z_][a-zA-Z0-9_]*", Name.Variable.Instance),

            # Script variables ($var)
            (r"\$[a-zA-Z_][a-zA-Z0-9_]*", Name.Variable),

            # Numbers
            (r"\d+\.\d+", Number.Float),
            (r"\d+", Number.Integer),

            # Identifiers
            (r"[a-zA-Z_][a-zA-Z0-9_]*", Name),

            # Punctuation
            (r"[{}()\[\]:,.]", Punctuation),

            # Operators
            (r"[+\-*/%]", Operator),

            # Whitespace
            (r"\s+", Text.Whitespace),
        ],

        "string": [
            # Interpolation: $$var
            (r"\$\$[a-zA-Z_][a-zA-Z0-9_]*", Name.Variable.Instance),
            # Interpolation: $var
            (r"\$[a-zA-Z_][a-zA-Z0-9_]*", Name.Variable),
            # Interpolation: ${$var} or ${$$var}
            (r"\$\{", String.Interpol, "interp"),
            # Escape sequences
            (r'\\[\\ntr"$]', String.Escape),
            # End of string
            (r'"', String.Double, "#pop"),
            # String content
            (r'[^"\\$]+', String.Double),
            (r"\$", String.Double),
        ],

        "interp": [
            (r"\$\$[a-zA-Z_][a-zA-Z0-9_]*", Name.Variable.Instance),
            (r"\$[a-zA-Z_][a-zA-Z0-9_]*", Name.Variable),
            (r"\}", String.Interpol, "#pop"),
        ],
    }
