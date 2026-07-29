#include "iox/ilic/IlicModelIndex.h"
#include "iox/ilic/ModelDef.h"

#include <string>
#include <algorithm>

namespace iox {
namespace ilic {

// ============================================================================
// IlicModelIndex::Impl
// ============================================================================

struct IlicModelIndex::Impl {
    const ModelDef* model = nullptr;
};

IlicModelIndex::IlicModelIndex(const ModelDef& model)
    : impl_(std::make_unique<Impl>()) {
    impl_->model = &model;
}

IlicModelIndex::~IlicModelIndex() = default;

const TopicDef* IlicModelIndex::findTopic(std::string_view scopedName) const {
    if (!impl_->model) return nullptr;
    return impl_->model->findTopic(scopedName);
}

const ClassDef* IlicModelIndex::findClass(std::string_view scopedName) const {
    if (!impl_->model) return nullptr;
    return impl_->model->findClass(scopedName);
}

const PropertyDef* IlicModelIndex::findProperty(
    const ClassDef& owner, std::string_view propertyName) const {
    return owner.findProperty(propertyName);
}

std::vector<const PropertyDef*> IlicModelIndex::transferProperties(
    const ClassDef& owner) const {
    std::vector<const PropertyDef*> result;
    for (auto& p : owner.properties) {
        result.push_back(&p);
    }
    // Sort by orderPos to ensure transfer order
    std::sort(result.begin(), result.end(),
        [](const PropertyDef* a, const PropertyDef* b) {
            return a->orderPos < b->orderPos;
        });
    return result;
}

} // namespace ilic
} // namespace iox
