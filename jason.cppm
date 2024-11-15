export module jason;

import hai;
import hashley;
import jute;
import silog;

namespace jason::token {
  enum type {
    error,
    l_bracket,
    r_bracket,
    l_brace,
    r_brace,
    string,
    colon,
    comma,
    boolean,
    null,
    number,
  };
  struct t {
    type type {};
    jute::view content {};
  };
  class list {
    hai::chain<t> m_list { 10240 };
    hai::chain<t>::const_iterator m_it;
  public:
    constexpr void push_back(t t) { m_list.push_back(t); }

    constexpr void reset() { m_it = static_cast<const hai::chain<t> &>(m_list).begin(); }

    constexpr auto peek() {
      if (!*this) silog::die("end of token list");
      return *m_it; 
    }
    constexpr auto take() {
      if (!*this) silog::die("end of token list");
      auto res = *m_it;
      auto _ = ++m_it;
      return res;
    }

    [[nodiscard]] constexpr operator bool() const {
      return m_it != m_list.end();
    }
  };
}

namespace jason {
  constexpr token::list tokenise(jute::view data) {
    token::list res {};
    while (data.size()) {
      const auto err = [&] (const char * msg) {
        silog::die("%s [%.*s]",
            msg,
            static_cast<unsigned>(data.size() > 20 ? 20 : data.size()),
            data.begin());
      };
      const auto push = [&] (auto t) {
        auto [l, r] = data.subview(1);
        res.push_back({ t, l });
        data = r;
      };
      const auto push_if = [&] (auto t, jute::view v) {
        if (data.starts_with(v)) {
          auto [l, r] = data.subview(v.size());
          res.push_back({ t, l });
          data = r;
        } else err("unexpected input starting at ");
      };
      switch (data[0]) { 
        case '[': push(token::l_bracket); break;
        case ']': push(token::r_bracket); break;
        case '{': push(token::l_brace); break;
        case '}': push(token::r_brace); break;
        case ':': push(token::colon); break;
        case ',': push(token::comma); break;
        case 't': push_if(token::boolean, "true"); break;
        case 'f': push_if(token::boolean, "false"); break;
        case 'n': push_if(token::null, "null"); break;
        case ' ':
        case '\t':
        case '\r':
        case '\n': data = data.subview(1).after; break;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9': {
          auto origin = data;
          while (data.size() && data[0] >= '0' && data[0] <= '9') {
            data = data.subview(0, 1).after;
          }
          if (data.size() && data[0] == '.') {
            data = data.subview(0, 1).after;
            while (data.size() && data[0] >= '0' && data[0] <= '9') {
              data = data.subview(0, 1).after;
            }
          }
          auto len = static_cast<unsigned>(data.begin() - origin.begin());
          jute::view val { origin.begin(), len };
          res.push_back({ token::number, val });
          break;
        }
        case '"': {
          auto origin = data;
          do {
            data = data.subview(1).after;
            if (data.size() && data[0] == '\\') data = data.subview(2).after;
          } while (data.size() && data[0] != '"');
  
          if (data.size() == 0) {
            data = origin;
            err("unmatched string started at ");
          }
          data = data.subview(1).after;
  
          auto len = static_cast<unsigned>(data.begin() - origin.begin());
          jute::view val { origin.begin(), len };
          res.push_back({ token::string, val });
          break;
        }
        default: err("unexpected input starting at ");
      }
    }
    return res;
  }
}
namespace jason::ast {
  enum type {
    error,
    dict,
    array,
    string,
    boolean,
    null,
    number,
  };
  class node {
    type m_type {};

  protected:
    explicit constexpr node(type t) : m_type { t } {}

  public:
    constexpr node() = default;
    constexpr virtual ~node() {}

    constexpr auto type() const { return m_type; }
  };
  template<type T>
  struct node_typed : public node {
    static constexpr const ast::type type = T;
    constexpr node_typed() : node { T } {}
  };

  [[nodiscard]] constexpr auto unescape(jute::view txt) {
    hai::array<char> buffer { static_cast<unsigned>(txt.size()) };
    auto ptr = buffer.begin();
    unsigned i;
    for (i = 1; i < txt.size() - 1; i++, ptr++) {
      if (txt[i] == '\\') *ptr = txt[++i];
      else *ptr = txt[i];
    }
    unsigned len = ptr - buffer.begin();
    return jute::heap { jute::view { buffer.begin(), len } };
  }
  static_assert(*unescape("\"asdf\"") == "asdf");
  static_assert(*unescape("\"a\\sdf\"") == "asdf");
  static_assert(*unescape("\"a\\\"sdf\"") == "a\"sdf");
}
export namespace jason::ast::nodes {
  class dict : public node_typed<ast::dict> {
    struct entry {
      jute::heap key;
      hai::uptr<node> value;
    };
    hai::chain<entry> m_values { 64 };
    hashley::niamh m_keys { 127 };
  public:
    constexpr void push_back(jute::heap key, node * value) {
      auto & k = m_keys[*key];
      if (k) silog::die("duplicate key found in dict");
      m_values.push_back({ key, hai::uptr { value }});
      k = m_values.size();
    }

    [[nodiscard]] constexpr bool has_key(jute::view key) const {
      return m_keys[key] != 0;
    }

    [[nodiscard]] constexpr auto & operator[](jute::view key) const {
      auto k = m_keys[key];
      if (!k) silog::die("key not found in dict");
      return m_values.seek(k - 1).value;
    }

    constexpr auto size() const { return m_values.size(); }
    constexpr auto begin() const { return m_values.begin(); }
    constexpr auto end() const { return m_values.end(); }
  };
  class array : public node_typed<ast::array> {
    hai::chain<hai::uptr<node>> m_data { 64 };
  public:
    constexpr void push_back(node * n) {
      m_data.push_back(hai::uptr { n });
    }

    [[nodiscard]] constexpr auto & operator[](unsigned idx) const {
      if (idx >= m_data.size()) silog::die("accessing element outside array bounds");
      return m_data.seek(idx);
    }

    constexpr auto size() const { return m_data.size(); }
    constexpr auto begin() const { return m_data.begin(); }
    constexpr auto end() const { return m_data.end(); }
  };
  class string : public node_typed<ast::string> {
    jute::view m_raw {};
  public:
    explicit constexpr string(jute::view cnt) : m_raw { cnt } {}
    constexpr auto raw() const { return m_raw; }
    constexpr auto str() const { return unescape(m_raw); }
  };
  class number : public node_typed<ast::number> {
    // Stored "raw" for easier conversion based on precision, etc
    jute::view m_raw {};
  public:
    explicit constexpr number(jute::view cnt) : m_raw { cnt } {}
    constexpr auto raw() const { return m_raw; }
    constexpr auto integer() const {
      auto d = m_raw;
      int res {};
      while (d.size()) {
        res = res * 10 + (d[0] - '0');
        d = d.subview(0, 1).after;
      }
      return res;
    }
  };
  class boolean : public node_typed<ast::boolean> {
    bool m_val {};
  public:
    explicit constexpr boolean(bool b) : m_val { b } {}
    constexpr operator bool() const { return m_val; }
  };
  class null : public node_typed<ast::null> {
  };
}
namespace jason::ast {
  constexpr node * parse(token::list & ts);
  constexpr node * parse_array(token::list & ts);
  constexpr node * parse_dict(token::list & ts);

  [[noreturn]] void fail(const char * msg, token::t t) {
    silog::die("%s: %.*s", msg, static_cast<int>(t.content.size()), t.content.begin());
  }

  constexpr node * parse_array(token::list & ts) {
    nodes::array res {};
    while (ts) {
      switch (ts.peek().type) {
        case token::r_bracket: 
          ts.take();
          return new nodes::array { traits::move(res) };
        default: res.push_back(parse(ts)); break;
      }
      
      switch (ts.take().type) {
        case token::r_bracket: return new nodes::array { traits::move(res) };
        case token::comma: continue;
        default: fail("invalid token while parsing array before", ts.peek());
      }
    }
    silog::die("end of file while parsing array");
  }
  constexpr node * parse_dict(token::list & ts) {
    nodes::dict res {};
    switch (ts.peek().type) {
      case token::r_brace:
        ts.take();
        return new nodes::dict { traits::move(res) };
      case token::string: 
        while (ts) {
          auto key = ts.take();
          if (ts.take().type != token::colon) fail("expecting colon after key", key);
          res.push_back(unescape(key.content), parse(ts));

          switch (ts.take().type) {
            case token::comma: continue;
            case token::r_brace: return new nodes::dict { traits::move(res) };
            default: fail("invalid token after dict entry", key);
          }
        }
        silog::die("end of file while parsing dict");
      default: fail("invalid token while parsing dict", ts.peek());
    }
  }
  constexpr node * parse(token::list & ts) {
    if (!ts) silog::die("eof trying to parse a value");

    auto [t, cnt] = ts.take();
    switch (t) {
      case token::l_brace: return parse_dict(ts);
      case token::l_bracket: return parse_array(ts);
      case token::boolean: return new nodes::boolean { "true" == cnt };
      case token::number: return new nodes::number { cnt };
      case token::string: return new nodes::string { cnt };
      case token::null: return new nodes::null {};
      default: fail("found token in a invalid position", {t, cnt});
    }
  }

  export template<typename N> constexpr const N & cast(const hai::uptr<node> & node) {
    if (node->type() != N::type) silog::die("expecting type %d got %d", N::type, node->type());
    return static_cast<const N &>(*node);
  }
}
namespace jason {
  constexpr auto parse(token::list & ts) {
    ts.reset();
    auto * res = ast::parse(ts);
    if (ts) ast::fail("extra tokens after valid value, starting from", ts.peek());
    return hai::uptr<ast::node> { res };
  }

  export constexpr auto parse(jute::view json) {
    auto tokens = tokenise(json);
    return parse(tokens);
  }
}

module :private;
static_assert([] {
  auto json = jason::parse(R"(
    { "hello": 1,
      "world": [ "!!", true ] }
  )");

  using namespace jason::ast::nodes;
  auto & world = cast<dict>(json)["world"];
  auto & t = cast<array>(world);
  return cast<boolean>(t[1]);
}());
