#include "iox/ilic/IlicModelIndex.h"

#include <functional>
#include <string>

namespace iox {
namespace ilic {

namespace {

bool matches(std::string_view requested, const std::string& qualified,
             const std::string& local) {
    return requested == qualified || requested == local;
}

const metamodel::SubModel* findTopicIn(const metamodel::Package& package,
                                       const std::string& prefix,
                                       std::string_view requested) {
    for (const auto* element : package.Element) {
        if (element == nullptr) continue;
        const auto qualified = prefix.empty() ? element->Name
                                              : prefix + "." + element->Name;
        if (const auto* topic = dynamic_cast<const metamodel::SubModel*>(element)) {
            if (matches(requested, qualified, topic->Name)) return topic;
        }
        if (const auto* child = dynamic_cast<const metamodel::Package*>(element)) {
            if (const auto* topic = findTopicIn(*child, qualified, requested)) return topic;
        }
    }
    return nullptr;
}

const metamodel::Class* findClassIn(const metamodel::Package& package,
                                    const std::string& prefix,
                                    std::string_view requested) {
    for (const auto* element : package.Element) {
        if (element == nullptr) continue;
        const auto qualified = prefix.empty() ? element->Name
                                              : prefix + "." + element->Name;
        if (const auto* klass = dynamic_cast<const metamodel::Class*>(element)) {
            if (matches(requested, qualified, klass->Name)) return klass;
        }
        if (const auto* child = dynamic_cast<const metamodel::Package*>(element)) {
            if (const auto* klass = findClassIn(*child, qualified, requested)) return klass;
        }
    }
    return nullptr;
}

} // namespace

struct IlicModelIndex::Impl {
    const metamodel::Model* model = nullptr;
};

IlicModelIndex::IlicModelIndex(const metamodel::Model& model)
    : impl_(std::make_unique<Impl>()) {
    impl_->model = &model;
}

IlicModelIndex::~IlicModelIndex() = default;

const metamodel::SubModel* IlicModelIndex::findTopic(
    std::string_view scopedName) const {
    if (impl_->model == nullptr) return nullptr;
    return findTopicIn(*impl_->model, impl_->model->Name, scopedName);
}

const metamodel::Class* IlicModelIndex::findClass(
    std::string_view scopedName) const {
    if (impl_->model == nullptr) return nullptr;
    return findClassIn(*impl_->model, impl_->model->Name, scopedName);
}

const metamodel::AttrOrParam* IlicModelIndex::findProperty(
    const metamodel::Class& owner, std::string_view propertyName) const {
    for (const auto* property : owner.ClassAttribute) {
        if (property != nullptr && property->Name == propertyName) return property;
    }
    return nullptr;
}

std::vector<const metamodel::AttrOrParam*> IlicModelIndex::transferProperties(
    const metamodel::Class& owner) const {
    std::vector<const metamodel::AttrOrParam*> result;
    result.reserve(owner.ClassAttribute.size());
    for (const auto* property : owner.ClassAttribute) {
        if (property != nullptr) result.push_back(property);
    }
    // ilic-core's schema-derived ClassAttribute list is already in the
    // normative declaration/transfer order. No synthetic order metadata is
    // invented here.
    return result;
}

} // namespace ilic
} // namespace iox
