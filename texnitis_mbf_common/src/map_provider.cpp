// MapProvider の実装。node ポインタごとにシングルトンを保持し、
// /map を 1 度だけ購読してその shared_ptr を全プラグインで共有する。

#include "texnitis_mbf_common/map_provider.hpp"

#include <utility>

namespace texnitis::mbf_common {

namespace {

/// node のポインタアドレスを weak_ptr テーブルのキーにする。
/// node が破棄されると entry の weak_ptr が expire し、次回 get()
/// 呼び出し時にエントリが置き換わる。
struct Registry {
    std::mutex                                                            mutex;
    std::unordered_map<rclcpp::Node *, std::weak_ptr<MapProvider>>        table;
};

Registry &registry () {
    static Registry r;
    return r;
}

}  // namespace

std::shared_ptr<MapProvider>
MapProvider::get (const rclcpp::Node::SharedPtr &node) {
    if (!node) {
        return nullptr;
    }
    auto &reg = registry ();
    std::lock_guard<std::mutex> guard (reg.mutex);
    auto &entry = reg.table[node.get ()];
    if (auto live = entry.lock ()) {
        return live;
    }
    auto provider = std::shared_ptr<MapProvider> (new MapProvider (node));
    entry         = provider;
    return provider;
}

void MapProvider::subscribe (const std::string &topic, size_t qos_depth) {
    {
        std::shared_lock<std::shared_mutex> read (mutex_);
        if (subs_.find (topic) != subs_.end ()) {
            return;
        }
    }

    Subscription sub;
    auto qos = rclcpp::QoS (rclcpp::KeepLast (qos_depth)).reliable ().transient_local ();
    sub.sub = node_->create_subscription<OccupancyGrid> (
        topic, qos,
        [this, topic] (OccupancyGrid::ConstSharedPtr msg) {
            std::unique_lock<std::shared_mutex> write (mutex_);
            auto it = subs_.find (topic);
            if (it == subs_.end ()) {
                return;
            }
            it->second.latest = msg;
        });

    std::unique_lock<std::shared_mutex> write (mutex_);
    subs_.emplace (topic, std::move (sub));
}

MapProvider::OccupancyGridPtr
MapProvider::latest (const std::string &topic) const {
    std::shared_lock<std::shared_mutex> read (mutex_);
    auto it = subs_.find (topic);
    if (it == subs_.end ()) {
        return nullptr;
    }
    return it->second.latest;
}

std::vector<std::string> MapProvider::subscribedTopics () const {
    std::shared_lock<std::shared_mutex> read (mutex_);
    std::vector<std::string> out;
    out.reserve (subs_.size ());
    for (const auto &kv : subs_) {
        out.push_back (kv.first);
    }
    return out;
}

}  // namespace texnitis::mbf_common
