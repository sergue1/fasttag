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

# SVG tags with lowercase names (Python name = tag_name.capitalize())
_SVG_SIMPLE_TAG_NAMES = [
    'circle', 'ellipse', 'line', 'path', 'polygon', 'polyline', 'rect',
    'g', 'defs', 'symbol', 'use', 'tspan', 'stop', 'marker', 'pattern',
    'mask', 'filter', 'view', 'animate', 'mpath',
]

# SVG tags with camelCase names: (Python export name, C extension name)
_SVG_CAMEL_TAG_NAMES = [
    ('Svg', 'Svg'),  # handled specially below
    ('SvgText', 'SvgText'),
    ('LinearGradient', 'LinearGradient'),
    ('RadialGradient', 'RadialGradient'),
    ('ClipPath', 'ClipPath'),
    ('ForeignObject', 'ForeignObject'),
    ('AnimateMotion', 'AnimateMotion'),
    ('AnimateTransform', 'AnimateTransform'),
    ('FeBlend', 'FeBlend'),
    ('FeColorMatrix', 'FeColorMatrix'),
    ('FeComponentTransfer', 'FeComponentTransfer'),
    ('FeComposite', 'FeComposite'),
    ('FeConvolveMatrix', 'FeConvolveMatrix'),
    ('FeDiffuseLighting', 'FeDiffuseLighting'),
    ('FeDisplacementMap', 'FeDisplacementMap'),
    ('FeDropShadow', 'FeDropShadow'),
    ('FeFlood', 'FeFlood'),
    ('FeFuncA', 'FeFuncA'),
    ('FeFuncB', 'FeFuncB'),
    ('FeFuncG', 'FeFuncG'),
    ('FeFuncR', 'FeFuncR'),
    ('FeGaussianBlur', 'FeGaussianBlur'),
    ('FeImage', 'FeImage'),
    ('FeMerge', 'FeMerge'),
    ('FeMergeNode', 'FeMergeNode'),
    ('FeMorphology', 'FeMorphology'),
    ('FeOffset', 'FeOffset'),
    ('FePointLight', 'FePointLight'),
    ('FeSpecularLighting', 'FeSpecularLighting'),
    ('FeSpotLight', 'FeSpotLight'),
    ('FeTile', 'FeTile'),
    ('FeTurbulence', 'FeTurbulence'),
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

# Export SVG simple tags
for tag_name in _SVG_SIMPLE_TAG_NAMES:
    capitalized = tag_name.capitalize()
    globals()[capitalized] = getattr(_fasttag, capitalized)

# Export SVG camelCase tags (special handling for Svg)
for py_name, c_name in _SVG_CAMEL_TAG_NAMES:
    if py_name == 'Svg':
        def Svg(*args, **kwargs):
            if 'xmlns' not in kwargs:
                kwargs['xmlns'] = 'http://www.w3.org/2000/svg'
            return _fasttag.Svg(*args, **kwargs)
        globals()['Svg'] = Svg
    else:
        globals()[py_name] = getattr(_fasttag, c_name)

_SVG_ALL_NAMES = (
    [t.capitalize() for t in _SVG_SIMPLE_TAG_NAMES] +
    [py_name for py_name, _ in _SVG_CAMEL_TAG_NAMES]
)

# Export all tag names
__all__ = ['HTML', 'DOCTYPE', 'Text', 'tag', 'set_indent'] + [tag.capitalize() for tag in _TAG_NAMES] + _SVG_ALL_NAMES
