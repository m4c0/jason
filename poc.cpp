#pragma leco tool

import hai;
import jojo;
import jute;
import silog;

namespace token {
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
    void push_back(t t) { m_list.push_back(t); }

    auto peek() { return *m_it; }
    auto take() {
      auto res = peek();
      auto _ = ++m_it;
      return res;
    }

    [[nodiscard]] constexpr operator bool() const {
      return m_it != m_list.end();
    }
  };
}

static constexpr token::list tokenise(jute::view data) {
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
        while (data.size() && data[0] >= '0' && data[1] <= '9') {
          data = data.subview(1).after;
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
namespace ast {
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
    virtual ~node() {}

    constexpr auto type() const { return m_type; }
  };
}
namespace ast::nodes {
  class dict : public node {
  };
  class array : public node {
  };
  class string : public node {
  };
  class number : public node {
  };
  class boolean : public node {
  };
  class null : public node {
  public:
    constexpr null() : node { ast::null } {}
  };
  class error : public node {
  public:
    constexpr error() : node { ast::error } {}
  };
}
auto parse(token::list & ts) {
  using res = hai::uptr<ast::node>;
  if (!ts) return res { new ast::nodes::error {} };

  auto [t, cnt] = ts.take();
  switch (t) {
    case token::null: return res { new ast::nodes::null {} };
    default: return res { new ast::nodes::error {} };
  }
}

int main() try {
  // Reads from GitHub's notification API
  jojo::read("out/test.json", nullptr, [](auto, const hai::array<char> & data) {
    auto tokens = tokenise(jute::view { data.begin(), data.size() });
    auto node = parse(tokens);
    silog::trace(node->type());
    if (tokens) silog::die("extra tokens");
  });
} catch (...) {
  return 1;
}
