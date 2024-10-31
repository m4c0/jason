#pragma leco tool

import jason;

import hai;
import jojo;
import jute;
import silog;

int main() try {
  // Reads from GitHub's notification API
  jojo::read("out/test.json", nullptr, [](auto, const hai::array<char> & data) {
    auto node = jason::parse(jute::view { data.begin(), data.size() });

    using namespace jason::ast::nodes;
    for (auto & n : cast<array>(node)) {
      auto & notif = cast<dict>(n);
      silog::trace("id", cast<string>(notif["id"]).str());
      silog::trace("reason", cast<string>(notif["reason"]).str());

      auto & subj = cast<dict>(notif["subject"]);
      silog::trace("title", cast<string>(subj["title"]).str());
    }
  });
} catch (...) {
  return 1;
}
