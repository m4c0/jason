#pragma leco tool

import jason;

import hai;
import jojo;
import jute;
import print;

int main() try {
  // Reads from GitHub's notification API
  jojo::read("out/test.json", nullptr, [](auto, const hai::array<char> & data) {
    auto node = jason::parse(jute::view { data.begin(), data.size() });

    using namespace jason::ast::nodes;
    for (auto & n : cast<array>(node)) {
      auto & notif = cast<dict>(n);
      auto & subj = cast<dict>(notif["subject"]);

      auto id = cast<string>(notif["id"]).str();
      jute::heap reason = cast<string>(notif["reason"]).str() + "                    ";
      auto title = cast<string>(subj["title"]).str();
      putln(id, ' ', (*reason).subview(0, 20).middle, title);
    }
  });
} catch (...) {
  return 1;
}
