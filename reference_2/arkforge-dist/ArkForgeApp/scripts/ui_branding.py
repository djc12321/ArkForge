"""Brand visible UI text while preserving URLs, code and interpolation tokens."""
import re
_PROTECTED = re.compile(r"(https?://[^\s<>)]+|command:[^\s<>)]+|`[^`]*`|\{\d+\})")
_BRAND = re.compile(r"(?<![A-Za-z0-9_])(?:Visual Studio Code|VS[ -]?Code|Code[ -]+OSS)(?![A-Za-z0-9_.])", re.I)
def brand_text(value):
    return ''.join(part if index % 2 else _BRAND.sub('ArkForge', part)
                   for index, part in enumerate(_PROTECTED.split(value)))
