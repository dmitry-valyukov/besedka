module besedka.app;

import std;
import besedka.forum;

namespace besedka::app {

std::vector<ShowcaseGroup> groupShowcase(const std::span<const forum::ForumDescription> forums) {
    std::vector<ShowcaseGroup> groups;

    for (std::size_t at = 0; at < forums.size(); ++at) {
        const forum::ForumGroup& group = forums[at].group;

        auto known = std::ranges::find_if(
            groups, [&group](const ShowcaseGroup& candidate) { return candidate.group.id == group.id; });

        if (known == groups.end()) {
            groups.push_back({group, {}});
            known = std::prev(groups.end());
        }

        known->forums.push_back(at);
    }

    // Устойчиво: группы с одинаковым sortOrder остаются в порядке появления.
    std::ranges::stable_sort(groups, {}, [](const ShowcaseGroup& group) { return group.group.sortOrder; });

    return groups;
}

std::vector<int> replyDepths(const forum::MessagePage& page) {
    std::vector<int> depths;

    depths.reserve(page.items.size());

    std::unordered_map<int, int> depthOf;

    for (const forum::Message& message : page.items) {
        const auto parent = depthOf.find(message.info.parentId);
        const int depth = parent == depthOf.end() ? 0 : parent->second + 1;

        depthOf.emplace(message.info.id, depth);
        depths.push_back(depth);
    }

    return depths;
}

}  // namespace besedka::app
