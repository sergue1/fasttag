import pickle
import fasttag
from fasttag import *
import fasthtml.common

fasttag.set_indent(-1)
def assert_equal(a, b): assert a == b, (a, b)
assert_equal(tag("div", "text&<>"), HTML('<div>text&amp;&lt;></div>'))
assert_equal(tag("span", "wow", a='Tom & Jerry "the mouse"'), HTML('<span a="Tom &amp; Jerry &quot;the mouse&quot;">wow</span>'))
assert_equal(tag("c", "cc", cls="a b c"), HTML('<c class="a b c">cc</c>'))
assert_equal(tag("b", "bb", hx_target="closest tr"), HTML('<b hx-target="closest tr">bb</b>'))
assert_equal(tag("input", type="text", value="value"), HTML('<input type="text" value="value">'))
assert_equal(Div("value"), HTML('<div>value</div>'))
assert_equal(Input(type="checkbox", checked=True, disabled=False), HTML('<input type="checkbox" checked>'))
fasttag.set_indent(2)
assert_equal(fasttag.Div("value"), HTML("<div>value</div>"))
print(fasttag.Tr(fasttag.Td("hello"), fasttag.Td("world")))
assert_equal(fasttag.Tr(fasttag.Td("hello"), fasttag.Td("world")), HTML("<tr>\n  <td>hello</td>\n  <td>world</td>\n</tr>"))
assert_equal(fasttag.Span("hello", "world"), HTML("<span>\n  hello\n  world\n</span>"))
assert_equal(fasttag.Span(I(cls="bi bi-person me-1"), "Anton"), HTML('<span><i class="bi bi-person me-1"></i>Anton</span>'))
assert_equal(fasttag.Td((Mark("Matched"), "Anton")), HTML('<td><mark>Matched</mark>Anton</td>'))
assert_equal(fasttag.Span("hello\nworld"), HTML("<span>\n  hello\n  world\n</span>"))
assert_equal(Input(type="number", value=10.3, min=-10, max=100), HTML('<input type="number" value="10.3" min="-10" max="100">'))
assert_equal(Div(3.3), HTML('<div>3.3</div>'))
assert_equal(str(HTML("aa") + HTML("BB")), "aaBB")
assert_equal(Div(HTML("aa")), HTML("<div>\n  aa\n</div>"))
assert_equal(Text("This is an <example> of text"), HTML("This is an &lt;example> of text"))
assert_equal(DOCTYPE, HTML("<!DOCTYPE html>\n"))
assert_equal(Div(_="value"), HTML('<div _="value"></div>'))
assert_equal(Div(_="value").__html__(), '<div _="value"></div>')
assert_equal(Div("value").tag, 'div')
assert_equal(Div("hello\nworld"), HTML('<div>\n  hello\n  world\n</div>'))
assert_equal(Pre("hello\nworld"), HTML('<pre>hello\nworld</pre>'))
assert_equal(Textarea("hello\nworld"), HTML('<textarea>hello\nworld</textarea>'))
assert_equal(Div(("hello", ("world", "nested"))), HTML('<div>helloworldnested</div>'))
assert_equal(Div(({"a": 2})), HTML("<div>{'a': 2}</div>"))
assert_equal(Div(["a", 2]), HTML("<div>['a', 2]</div>"))
assert_equal(Div(a=[1,2]), HTML('<div a="[1, 2]"></div>'))
assert_equal(Div(None, cls="small"), HTML('<div class="small"></div>'))
assert_equal(Div(a=["'",'"']), HTML('''<div a="[&quot;'&quot;, '&quot;']"></div>'''))
assert_equal(fasthtml.common.to_xml(fasthtml.common.Div(fasthtml.common.Span("hello"))), '<div>\n<span>hello</span></div>\n')
assert_equal(Div("value", a="b", ccc="d", and2="tom&jerry").attrs,
             {"a": "b", "ccc": "d", "and2": "tom&jerry"})
assert_equal(str(A(hx_get="/")), '<a hx-get="/" href="#"></a>')

assert_equal(Div(
    Label('User', _for='user', cls='form-label'),
    Select(
        Option(style='display:none'),
        Option('2', value='2'),
        Option('3', value='3'),
        id='user',
        name='user',
        data_placeholder='Type something...',
        cls='form-select visible'
    ),
    Div(id='user_err', cls='invalid-feedback'),
    cls='col-md-9 mb-3'
), HTML("""\
<div class="col-md-9 mb-3">
  <label for="user" class="form-label">User</label>
  <select id="user" name="user" data-placeholder="Type something..." class="form-select visible">
    <option style="display:none"></option>
    <option value="2">2</option>
    <option value="3">3</option>
  </select>
  <div id="user_err" class="invalid-feedback"></div>
</div>"""))

assert_equal(Script("""\
if (window.innerWidth < 768) {
    sidebar.classList.remove('show');
}"""), HTML("""<script>
  if (window.innerWidth < 768) {
      sidebar.classList.remove('show');
  }
</script>"""))

class HTML_Test:
    def __html__(self):
        return "hello"

assert_equal(Div(HTML_Test()), HTML("<div>hello</div>"))

a = HTML("<p>hello</p>")
assert_equal(pickle.loads(pickle.dumps(a)), a)


print(
    Div(
        Div(Label("First Name"), ": Joe"),
        Div(Label("Last Name"), ": Blow"),
        Div(Label("Email"), ": joe@blow.com"),
        # if a keyword starts with _, the first _ is ignored and the rest is used as an argument unchanged.
        Button("Click To Edit", hx_get="/contact/1/edit", cls="btn primary"),
        # If the keyword argument doesn't start with _, underscores are converted to hypens (-) in the attibute name
        hx_target="this", hx_swap="outerHTML")
    )


ss = """<tr id="e8685253296095281136" style="background-color: #d7a748">
  <td>23</td>
  <td>
    <a href="/city/Molino d'Elsa">Molino d'Elsa (IT)</a>
  </td>
  <td>
    <span>
      18.14°C
      <br>
      (19°C / 16°C)
    </span>
  </td>
  <td>1</td>
  <td>0</td>
  <td>
    <a href="#" hx-post="/?city=Molino d'Elsa">update</a>
  </td>
  <td>43.3243</td>
</tr>"""

assert_equal(str(HTML(ss)), ss)

# Test callable syntax (attribute-first pattern)
print("Testing callable syntax...")
assert_equal(Div(cls="small")(Span("text"), Div("1")), Div(Span("text"), Div("1"), cls="small"))
assert_equal(Div(cls="container", id="main")(Span("content")), Div(Span("content"), cls="container", id="main"))
assert_equal(Div(cls="outer")(Span("inner"), id="wrapper"), Div(Span("inner"), cls="outer", id="wrapper"))
assert_equal(Div(cls="empty")(), Div(cls="empty"))
assert_equal(Div(cls="empty")(), Div(cls="empty"))
print("Callable syntax tests passed! ✓")

# Test SVG namespace support
print("Testing SVG namespace support...")
fasttag.set_indent(-1)

# __ -> : in attribute names (namespace separator)
assert_equal(tag("use", xlink__href="#icon"), HTML('<use xlink:href="#icon"></use>'))
assert_equal(tag("svg", xmlns__xlink="http://www.w3.org/1999/xlink"), HTML('<svg xmlns:xlink="http://www.w3.org/1999/xlink"></svg>'))
assert_equal(tag("text", xml__space="preserve"), HTML('<text xml:space="preserve"></text>'))

# SVG element tags
assert_equal(Circle(cx="50", cy="50", r="40"), HTML('<circle cx="50" cy="50" r="40"></circle>'))
assert_equal(Rect(width="100", height="100", fill="blue"), HTML('<rect width="100" height="100" fill="blue"></rect>'))
assert_equal(Path(d="M 0 0 L 100 100"), HTML('<path d="M 0 0 L 100 100"></path>'))
assert_equal(Line(x1="0", y1="0", x2="100", y2="100"), HTML('<line x1="0" y1="0" x2="100" y2="100"></line>'))
assert_equal(G(Circle(r="10"), id="group"), HTML('<g id="group"><circle r="10"></circle></g>'))
assert_equal(SvgText("Hello", x="10", y="20"), HTML('<text x="10" y="20">Hello</text>'))
assert_equal(LinearGradient(id="grad"), HTML('<linearGradient id="grad"></linearGradient>'))
assert_equal(ClipPath(id="clip"), HTML('<clipPath id="clip"></clipPath>'))
assert_equal(ForeignObject(width="100", height="100"), HTML('<foreignObject width="100" height="100"></foreignObject>'))

# Svg auto-adds xmlns
assert_equal(Svg(Circle(r="10")), HTML('<svg xmlns="http://www.w3.org/2000/svg"><circle r="10"></circle></svg>'))
# Svg: explicit xmlns overrides default
assert_equal(Svg(xmlns="http://www.w3.org/2000/svg"), HTML('<svg xmlns="http://www.w3.org/2000/svg"></svg>'))

print("SVG namespace tests passed! ✓")
