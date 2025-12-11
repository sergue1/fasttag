"""
FastTag - Extremely fast HTML tag generator with FastHTML-compatible callable syntax
"""
from . import _fasttag

# Export HTML type and other utilities from C extension
HTML = _fasttag.HTML
DOCTYPE = _fasttag.DOCTYPE
Text = _fasttag.Text
tag = _fasttag.tag
set_indent = _fasttag.set_indent

# List of all HTML tags to export
_TAG_NAMES = [
    'a', 'abbr', 'address', 'area', 'article', 'aside', 'audio',
    'b', 'base', 'bdi', 'bdo', 'blockquote', 'body', 'br', 'button',
    'canvas', 'caption', 'cite', 'code', 'col', 'colgroup',
    'data', 'datalist', 'dd', 'del', 'details', 'dfn', 'dialog', 'div', 'dl', 'dt',
    'em', 'embed',
    'fieldset', 'figcaption', 'figure', 'footer', 'form',
    'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'head', 'header', 'hgroup', 'hr', 'html',
    'i', 'iframe', 'img', 'input', 'ins',
    'kbd',
    'label', 'legend', 'li', 'link',
    'main', 'map', 'mark', 'meta', 'meter',
    'nav', 'noscript',
    'object', 'ol', 'optgroup', 'option', 'output',
    'p', 'param', 'picture', 'pre', 'progress',
    'q',
    'rp', 'rt', 'ruby',
    's', 'samp', 'script', 'section', 'select', 'small', 'source', 'span', 'strong', 'style', 'sub', 'summary', 'sup',
    'table', 'tbody', 'td', 'template', 'textarea', 'tfoot', 'th', 'thead', 'time', 'title', 'tr', 'track',
    'u', 'ul',
    'var', 'video',
    'wbr',
    # Deprecated tags
    'acronym', 'applet', 'basefont', 'bgsound', 'big', 'blink', 'center', 'content', 'dir', 'element',
    'font', 'frame', 'frameset', 'image', 'isindex', 'keygen', 'listing', 'marquee', 'menu', 'menuitem',
    'multicol', 'nextid', 'nobr', 'noembed', 'noframes', 'plaintext', 'shadow', 'spacer', 'strike', 'tt', 'xmp'
]

# Directly export C functions
for tag_name in _TAG_NAMES:
    capitalized = tag_name.capitalize()
    c_func = getattr(_fasttag, capitalized)

    # Special handling for A tag: add default href="#"
    if tag_name == 'a':
        def A(*args, **kwargs):
            if 'href' not in kwargs:
                kwargs['href'] = '#'
            return _fasttag.A(*args, **kwargs)
        globals()['A'] = A
    else:
        globals()[capitalized] = c_func

# Export all tag names
__all__ = ['HTML', 'DOCTYPE', 'Text', 'tag', 'set_indent'] + [tag.capitalize() for tag in _TAG_NAMES]
