#include "iox/ilic/IlicModelIndex.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace iox {
namespace ilic {
namespace {

constexpr std::string_view xtf24ModelNamespaceBase =
    "http://www.interlis.ch/xtf/2.4/";

const metamodel::MetaElement* translationRoot(
    const metamodel::MetaElement* element) {
    std::unordered_set<const metamodel::MetaElement*> seen;
    while (element != nullptr && element->_translationOf != nullptr) {
        if (!seen.insert(element).second) {
            throw IoxError(DiagnosticCode::ModelMismatch,
                           "Cycle in ilic translation relationships");
        }
        element = element->_translationOf;
    }
    return element;
}

const metamodel::Class* ownerClass(const metamodel::MetaElement& element) {
    if (const auto* attribute =
            dynamic_cast<const metamodel::AttrOrParam*>(&element)) {
        return attribute->AttrParent;
    }
    if (const auto* role = dynamic_cast<const metamodel::Role*>(&element)) {
        return role->Association;
    }
    return dynamic_cast<const metamodel::Class*>(element.ElementInPackage);
}

const metamodel::Model* containingModel(
    const metamodel::MetaElement& element) {
    const metamodel::MetaElement* current = &element;
    if (const auto* owner = ownerClass(element); owner != nullptr) {
        current = owner;
    }
    while (current != nullptr) {
        if (const auto* model =
                dynamic_cast<const metamodel::Model*>(current)) {
            return model;
        }
        current = current->ElementInPackage;
    }
    return nullptr;
}

std::string scopedName(const metamodel::MetaElement& element) {
    std::vector<std::string> components;
    const metamodel::MetaElement* current = &element;
    while (current != nullptr) {
        if (!current->Name.empty()) components.push_back(current->Name);
        current = current->ElementInPackage;
    }
    std::string result;
    for (auto iterator = components.rbegin(); iterator != components.rend();
         ++iterator) {
        if (!result.empty()) result.push_back('.');
        result.append(*iterator);
    }
    return result;
}

std::string modelNamespace(const metamodel::Model& model) {
    if (!model.xmlns.empty()) return model.xmlns;
    return std::string(xtf24ModelNamespaceBase) + model.Name;
}

int normalizedRoleMaximum(const metamodel::Role& role) {
    if (role.Multiplicity.Max >= 0) return role.Multiplicity.Max;
    return role.Strongness == metamodel::Role::Comp ? 1 : -1;
}

bool standaloneAssociation(const metamodel::Class& association) {
    if (association.Kind != metamodel::Class::Association) return true;
    const auto hasAttributes = std::any_of(
        association.ClassAttribute.begin(), association.ClassAttribute.end(),
        [](const auto* attribute) {
            return attribute != nullptr && !attribute->Transient;
        });
    if (association.Role.size() > 2U) return true;
    if (association.Role.size() < 2U) return hasAttributes;
    auto iterator = association.Role.begin();
    const auto* first = *iterator++;
    const auto* second = *iterator;
    if (first == nullptr || second == nullptr) return hasAttributes;
    const auto firstMaximum = normalizedRoleMaximum(*first);
    const auto secondMaximum = normalizedRoleMaximum(*second);
    const bool firstExactlyOne = first->Multiplicity.Min == 1 &&
                                 firstMaximum == 1;
    const bool secondExactlyOne = second->Multiplicity.Min == 1 &&
                                  secondMaximum == 1;
    if (firstExactlyOne || secondExactlyOne) return false;
    if (hasAttributes) return true;
    return (firstMaximum < 0 || firstMaximum > 1) &&
           (secondMaximum < 0 || secondMaximum > 1);
}

const metamodel::EnumType* enumerationType(const metamodel::Type* type) {
    std::unordered_set<const metamodel::Type*> seen;
    while (type != nullptr && seen.insert(type).second) {
        if (const auto* enumeration =
                dynamic_cast<const metamodel::EnumType*>(type)) {
            return enumeration;
        }
        if (const auto* tree =
                dynamic_cast<const metamodel::EnumTreeValueType*>(type)) {
            return tree->ET;
        }
        if (const auto* related =
                dynamic_cast<const metamodel::TypeRelatedType*>(type)) {
            type = related->BaseType;
            continue;
        }
        if (type->_other_type != nullptr) {
            type = type->_other_type;
            continue;
        }
        type = dynamic_cast<const metamodel::Type*>(type->Super);
    }
    return nullptr;
}

template<typename Value>
void appendUnique(std::vector<Value>& values, Value value) {
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(std::move(value));
    }
}

} // namespace

struct IlicModelIndex::Impl final {
    struct ModelInfo final {
        std::string name;
        std::string language;
        std::string version;
        std::string uri;
        std::string wireNamespace;
    };

    struct NameVariant final {
        std::string model;
        std::string language;
        std::string scoped;
        std::string local;
        XmlQualifiedName xml;
    };

    struct TopicConcept final { std::vector<NameVariant> variants; };

    struct ClassVariant final {
        NameVariant name;
        std::vector<std::size_t> allProperties;
        std::vector<std::size_t> transferProperties;
    };

    struct ClassConcept final {
        std::vector<ClassVariant> variants;
        bool topLevelTransferable = false;
    };

    struct EnumVariant final {
        std::string model;
        std::string language;
        std::string lexical;
    };

    struct EnumConcept final { std::vector<EnumVariant> variants; };

    struct PropertyVariant final {
        NameVariant name;
        std::vector<std::pair<std::string, std::size_t>> enumerations;
    };

    struct PropertyConcept final {
        std::vector<PropertyVariant> variants;
        bool transient = false;
        bool role = false;
        bool embedded = false;
        std::optional<std::size_t> targetClass;
        bool hasEnumeration = false;
    };

    struct Lookup final {
        std::size_t conceptIndex = 0;
        std::optional<std::size_t> variant;
    };

    std::vector<ModelInfo> models;
    std::vector<TopicConcept> topics;
    std::vector<ClassConcept> classes;
    std::vector<PropertyConcept> properties;
    std::vector<EnumConcept> enumerations;

    static const NameVariant& variantName(const NameVariant& variant) {
        return variant;
    }

    static const NameVariant& variantName(const ClassVariant& variant) {
        return variant.name;
    }

    static const NameVariant& variantName(const PropertyVariant& variant) {
        return variant.name;
    }

    [[noreturn]] static void ambiguous(std::string_view kind,
                                       const IomName& name) {
        throw IoxError(
            DiagnosticCode::ModelMismatch,
            "Ambiguous ilic " + std::string(kind) + " mapping for " +
                (name.hasInterlisName() ? name.interlisName()
                                        : name.xmlName().expanded()));
    }

    std::size_t modelIndex(std::string_view name) const {
        std::optional<std::size_t> result;
        for (std::size_t index = 0; index < models.size(); ++index) {
            if (models[index].name != name) continue;
            if (result) {
                throw IoxError(DiagnosticCode::ModelMismatch,
                               "Ambiguous ilic model name: " +
                                   std::string(name));
            }
            result = index;
        }
        if (!result) {
            throw IoxError(DiagnosticCode::ModelMismatch,
                           "Unknown ilic target model: " +
                               std::string(name));
        }
        return *result;
    }

    template<typename Concept>
    static std::optional<Lookup> lookup(
        const IomName& observed, const std::vector<Concept>& concepts,
        std::string_view kind) {
        std::vector<Lookup> nameMatches;
        std::vector<Lookup> xmlMatches;
        const bool scoped = observed.hasInterlisName() &&
                            observed.interlisName().find('.') !=
                                std::string::npos;
        for (std::size_t conceptIndex = 0; conceptIndex < concepts.size();
             ++conceptIndex) {
            const auto& variants = concepts[conceptIndex].variants;
            for (std::size_t variantIndex = 0;
                 variantIndex < variants.size(); ++variantIndex) {
                const auto& variant = variants[variantIndex];
                const auto& name = variantName(variant);
                if (observed.hasInterlisName()) {
                    const bool matched = scoped
                                             ? name.scoped ==
                                                   observed.interlisName()
                                             : name.local ==
                                                   observed.interlisName();
                    if (matched) {
                        nameMatches.push_back(
                            {conceptIndex, variantIndex});
                    }
                }
                if (observed.hasXmlName()) {
                    if (name.xml == observed.xmlName()) {
                        xmlMatches.push_back(
                            {conceptIndex, variantIndex});
                    }
                }
            }
        }

        const auto collapse = [&](const std::vector<Lookup>& matches)
            -> std::optional<Lookup> {
            if (matches.empty()) return std::nullopt;
            const auto conceptIndex = matches.front().conceptIndex;
            if (std::any_of(
                    matches.begin(), matches.end(), [&](const auto& item) {
                        return item.conceptIndex != conceptIndex;
                    })) {
                ambiguous(kind, observed);
            }
            std::optional<std::size_t> variant = matches.front().variant;
            for (const auto& item : matches) {
                if (item.variant != variant) variant.reset();
            }
            return Lookup{conceptIndex, variant};
        };
        const auto byName = collapse(nameMatches);
        const auto byXml = collapse(xmlMatches);
        if (byName && byXml && byName->conceptIndex != byXml->conceptIndex) {
            ambiguous(kind, observed);
        }
        if (byName && byXml) {
            return byName->variant ? byName : byXml;
        }
        return byName ? byName : byXml;
    }

    template<typename Variant>
    const Variant& chooseVariant(const std::vector<Variant>& variants,
                                 const Lookup& source,
                                 std::string_view targetModel) const {
        if (targetModel.empty()) {
            if (source.variant && *source.variant < variants.size()) {
                return variants[*source.variant];
            }
            throw IoxError(DiagnosticCode::ModelMismatch,
                           "A target model is required for an ambiguous "
                           "translation mapping");
        }
        const auto& target = models[modelIndex(targetModel)];
        const Variant* result = nullptr;
        for (const auto& variant : variants) {
            const auto& name = [&]() -> const NameVariant& {
                if constexpr (std::is_same_v<Variant, NameVariant>) {
                    return variant;
                } else {
                    return variant.name;
                }
            }();
            if (name.model == target.name) return variant;
            if (name.language != target.language) continue;
            if (result != nullptr) {
                throw IoxError(DiagnosticCode::ModelMismatch,
                               "Multiple ilic translations use language " +
                                   target.language);
            }
            result = &variant;
        }
        if (result == nullptr) {
            throw IoxError(DiagnosticCode::ModelMismatch,
                           "No ilic translation exists for target model " +
                               target.name);
        }
        return *result;
    }

    static IomName iomName(const NameVariant& variant, XtfVersion version,
                           bool scoped) {
        const auto interlisName = scoped ? variant.scoped : variant.local;
        if (version == XtfVersion::V24) {
            return IomName(interlisName, variant.xml);
        }
        return IomName(interlisName);
    }

    std::optional<Lookup> propertyLookup(const Lookup& owner,
                                         const IomName& observed) const {
        std::vector<std::size_t> candidates;
        for (const auto& variant : classes[owner.conceptIndex].variants) {
            for (const auto property : variant.allProperties) {
                appendUnique(candidates, property);
            }
        }
        std::vector<PropertyConcept> subset;
        subset.reserve(candidates.size());
        for (const auto candidate : candidates) {
            subset.push_back(properties[candidate]);
        }
        const auto found = lookup(observed, subset, "property");
        if (!found) return std::nullopt;
        return Lookup{candidates[found->conceptIndex], found->variant};
    }

    std::optional<std::size_t> enumConcept(
        const PropertyConcept& property, const Lookup& propertySource,
        std::string_view lexical) const {
        std::vector<std::size_t> matches;
        const auto inspect = [&](const PropertyVariant& variant) {
            for (const auto& value : variant.enumerations) {
                if (value.first == lexical) appendUnique(matches, value.second);
            }
        };
        if (propertySource.variant &&
            *propertySource.variant < property.variants.size()) {
            inspect(property.variants[*propertySource.variant]);
        } else {
            for (const auto& variant : property.variants) inspect(variant);
        }
        if (matches.empty()) return std::nullopt;
        if (matches.size() != 1U) {
            throw IoxError(DiagnosticCode::ModelMismatch,
                           "Ambiguous translated enumeration value: " +
                               std::string(lexical));
        }
        return matches.front();
    }
};

IlicModelIndex::IlicModelIndex(const metamodel::MetaModelStore& store)
    : impl_(std::make_unique<Impl>()) {
    using ElementMap =
        std::unordered_map<const metamodel::MetaElement*, std::size_t>;
    ElementMap topicConcepts;
    ElementMap classConcepts;
    ElementMap propertyConcepts;
    ElementMap enumConcepts;
    std::unordered_map<const metamodel::Class*,
                       std::pair<std::size_t, std::size_t>> classVariants;
    std::vector<const metamodel::Class*> concreteClasses;

    const auto conceptFor = [](ElementMap& map,
                               const metamodel::MetaElement& element,
                               auto& concepts) {
        const auto* root = translationRoot(&element);
        const auto found = map.find(root);
        if (found != map.end()) return found->second;
        const auto index = concepts.size();
        concepts.emplace_back();
        map.emplace(root, index);
        return index;
    };

    for (const auto* model : store.models()) {
        if (model == nullptr) continue;
        const auto* root = dynamic_cast<const metamodel::Model*>(
            translationRoot(model));
        if (root == nullptr) {
            throw IoxError(DiagnosticCode::ModelMismatch,
                           "Translated ilic model has a non-model root");
        }
        const auto rootNamespace = modelNamespace(*root);
        impl_->models.push_back({model->Name, model->Language,
                                 model->Version, model->At,
                                 rootNamespace});
    }

    const auto makeVariant = [&](const metamodel::MetaElement& element,
                                 const metamodel::Model& model,
                                 std::string scoped) {
        const auto* root = translationRoot(&element);
        const auto* rootModel = containingModel(*root);
        if (rootModel == nullptr) rootModel = &model;
        Impl::NameVariant variant;
        variant.model = model.Name;
        variant.language = model.Language;
        variant.scoped = std::move(scoped);
        variant.local = element.Name;
        variant.xml = {modelNamespace(*rootModel), root->Name, model.Name};
        return variant;
    };

    const auto visitPackage = [&](const auto& self,
                                  const metamodel::Package& package,
                                  const metamodel::Model& model) -> void {
        for (const auto* element : package.Element) {
            if (element == nullptr) continue;
            if (const auto* topic =
                    dynamic_cast<const metamodel::SubModel*>(element)) {
                const auto conceptIndex = conceptFor(topicConcepts, *topic,
                                                     impl_->topics);
                impl_->topics[conceptIndex].variants.push_back(
                    makeVariant(*topic, model, scopedName(*topic)));
                self(self, *topic, model);
                continue;
            }
            if (const auto* klass =
                    dynamic_cast<const metamodel::Class*>(element)) {
                const auto conceptIndex = conceptFor(classConcepts, *klass,
                                                     impl_->classes);
                Impl::ClassVariant variant;
                variant.name =
                    makeVariant(*klass, model, scopedName(*klass));
                const auto variantIndex =
                    impl_->classes[conceptIndex].variants.size();
                impl_->classes[conceptIndex].variants.push_back(
                    std::move(variant));
                classVariants.emplace(klass,
                                      std::make_pair(conceptIndex, variantIndex));
                concreteClasses.push_back(klass);
            }
            if (const auto* child =
                    dynamic_cast<const metamodel::Package*>(element)) {
                self(self, *child, model);
            }
        }
    };

    for (const auto* model : store.models()) {
        if (model != nullptr) visitPackage(visitPackage, *model, *model);
    }

    const auto registerEnumeration = [&](const auto& self,
                                         const metamodel::EnumNode& parent,
                                         std::string prefix,
                                         const metamodel::Model& model,
                                         Impl::PropertyVariant& property) -> void {
        for (const auto* node : parent.Node) {
            if (node == nullptr) continue;
            const auto lexical = prefix.empty() ? node->Name
                                                : prefix + "." + node->Name;
            const auto conceptIndex = conceptFor(enumConcepts, *node,
                                                 impl_->enumerations);
            impl_->enumerations[conceptIndex].variants.push_back(
                {model.Name, model.Language, lexical});
            property.enumerations.push_back({lexical, conceptIndex});
            self(self, *node, lexical, model, property);
        }
    };

    const auto registerProperty = [&](const metamodel::MetaElement& property,
                                      const metamodel::Class& owner) {
        const auto* model = containingModel(owner);
        if (model == nullptr) {
            throw IoxError(DiagnosticCode::ModelMismatch,
                           "ilic property owner has no model");
        }
        const auto conceptIndex = conceptFor(propertyConcepts, property,
                                             impl_->properties);
        auto& record = impl_->properties[conceptIndex];
        Impl::PropertyVariant variant;
        variant.name = makeVariant(
            property, *model, scopedName(owner) + "." + property.Name);
        const auto duplicate = std::find_if(
            record.variants.begin(), record.variants.end(),
            [&](const auto& item) {
                return item.name.model == variant.name.model;
            });
        if (duplicate != record.variants.end()) return conceptIndex;
        if (const auto* attribute =
                dynamic_cast<const metamodel::AttrOrParam*>(&property)) {
            record.transient = record.transient || attribute->Transient;
            if (const auto* enumeration = enumerationType(attribute->Type);
                enumeration != nullptr && enumeration->TopNode != nullptr) {
                record.hasEnumeration = true;
                registerEnumeration(registerEnumeration,
                                    *enumeration->TopNode, {}, *model,
                                    variant);
            }
        }
        if (const auto* role =
                dynamic_cast<const metamodel::Role*>(&property)) {
            record.role = true;
            record.embedded = record.embedded || role->EmbeddedTransfer ||
                              (role->Association != nullptr &&
                               role->Association->EmbeddedRoleTransfer);
            if (role->_baseclass != nullptr) {
                const auto target = classVariants.find(role->_baseclass);
                if (target != classVariants.end()) {
                    if (record.targetClass &&
                        *record.targetClass != target->second.first) {
                        throw IoxError(
                            DiagnosticCode::ModelMismatch,
                            "Translated role has conflicting target classes");
                    }
                    record.targetClass = target->second.first;
                }
            }
        }
        record.variants.push_back(std::move(variant));
        return conceptIndex;
    };

    for (const auto* klass : concreteClasses) {
        if (klass == nullptr) continue;
        for (const auto* role : klass->Role) {
            if (role != nullptr) (void)registerProperty(*role, *klass);
        }
        for (const auto* attribute : klass->ClassAttribute) {
            if (attribute != nullptr) {
                (void)registerProperty(*attribute, *klass);
            }
        }
        for (const auto* role : klass->_roleaccess) {
            if (role != nullptr && role->Association != nullptr) {
                (void)registerProperty(*role, *role->Association);
            }
        }
    }

    const auto appendProperty = [&](std::vector<std::size_t>& result,
                                    const metamodel::MetaElement* property,
                                    const metamodel::Class& owner,
                                    bool includeTransient) {
        if (property == nullptr) return;
        const auto conceptIndex = registerProperty(*property, owner);
        if (!includeTransient && impl_->properties[conceptIndex].transient) return;
        const auto found = std::find(result.begin(), result.end(), conceptIndex);
        if (found == result.end()) result.push_back(conceptIndex);
    };

    const auto classOrder = [&](const metamodel::Class& concrete,
                                bool includeTransient) {
        std::vector<const metamodel::Class*> hierarchy;
        const metamodel::Class* current = &concrete;
        std::unordered_set<const metamodel::Class*> seen;
        while (current != nullptr && seen.insert(current).second) {
            hierarchy.push_back(current);
            current = dynamic_cast<const metamodel::Class*>(current->Super);
        }
        std::reverse(hierarchy.begin(), hierarchy.end());
        std::vector<std::size_t> result;
        for (const auto* klass : hierarchy) {
            if (klass->Kind == metamodel::Class::Association) {
                for (const auto* role : klass->Role) {
                    appendProperty(result, role, *klass, includeTransient);
                }
                for (const auto* role : klass->_roleaccess) {
                    appendProperty(result, role,
                                   role != nullptr && role->Association != nullptr
                                       ? *role->Association
                                       : *klass,
                                   includeTransient);
                }
            }
            for (const auto* attribute : klass->ClassAttribute) {
                appendProperty(result, attribute, *klass, includeTransient);
            }
            if (klass->Kind != metamodel::Class::Association) {
                for (const auto* role : klass->_roleaccess) {
                    appendProperty(result, role,
                                   role != nullptr && role->Association != nullptr
                                       ? *role->Association
                                       : *klass,
                                   includeTransient);
                }
            }
        }
        return result;
    };

    for (const auto* klass : concreteClasses) {
        const auto found = classVariants.find(klass);
        if (found == classVariants.end()) continue;
        auto& classConcept = impl_->classes[found->second.first];
        auto& variant = classConcept.variants[found->second.second];
        variant.allProperties = classOrder(*klass, true);
        variant.transferProperties = classOrder(*klass, false);

        bool transferable = !klass->Abstract &&
                            klass->Kind != metamodel::Class::Structure;
        if (const auto* view = dynamic_cast<const metamodel::View*>(klass)) {
            transferable = transferable && !view->Transient;
        }
        if (klass->Kind == metamodel::Class::Association) {
            transferable = transferable && standaloneAssociation(*klass);
        }
        classConcept.topLevelTransferable =
            classConcept.topLevelTransferable || transferable;
    }
}

IlicModelIndex::~IlicModelIndex() = default;

std::optional<std::string> IlicModelIndex::modelLanguage(
    std::string_view modelName) const {
    const Impl::ModelInfo* result = nullptr;
    for (const auto& model : impl_->models) {
        if (model.name != modelName) continue;
        if (result != nullptr) {
            throw IoxError(DiagnosticCode::ModelMismatch,
                           "Ambiguous ilic model name: " +
                               std::string(modelName));
        }
        result = &model;
    }
    if (result == nullptr) return std::nullopt;
    return result->language;
}

std::optional<ModelEntry> IlicModelIndex::transferModel(
    std::string_view modelName, XtfVersion version) const {
    std::optional<std::size_t> index;
    for (std::size_t current = 0; current < impl_->models.size(); ++current) {
        if (impl_->models[current].name != modelName) continue;
        if (index) {
            throw IoxError(DiagnosticCode::ModelMismatch,
                           "Ambiguous ilic model name: " +
                               std::string(modelName));
        }
        index = current;
    }
    if (!index) return std::nullopt;
    const auto& model = impl_->models[*index];
    ModelEntry result;
    result.name = model.name;
    if (version == XtfVersion::V23) {
        if (!model.version.empty()) result.version = model.version;
        if (!model.uri.empty()) result.uri = model.uri;
    } else {
        result.xmlNamespace = {model.wireNamespace, model.name, model.name};
    }
    return result;
}

std::optional<IomName> IlicModelIndex::resolveTopic(
    const IomName& observed, std::string_view targetModel,
    XtfVersion version) const {
    const auto source = Impl::lookup(observed, impl_->topics, "topic");
    if (!source) return std::nullopt;
    const auto& variant = impl_->chooseVariant(
        impl_->topics[source->conceptIndex].variants, *source, targetModel);
    return Impl::iomName(variant, version, true);
}

std::optional<IomName> IlicModelIndex::resolveClass(
    const IomName& observed, std::string_view targetModel,
    XtfVersion version) const {
    const auto source = Impl::lookup(observed, impl_->classes, "class");
    if (!source) return std::nullopt;
    const auto& variant = impl_->chooseVariant(
        impl_->classes[source->conceptIndex].variants, *source, targetModel);
    return Impl::iomName(variant.name, version, true);
}

std::optional<IomName> IlicModelIndex::resolveProperty(
    const IomName& owner, const IomName& observed,
    std::string_view targetModel, XtfVersion version) const {
    const auto ownerSource = Impl::lookup(owner, impl_->classes, "class");
    if (!ownerSource) return std::nullopt;
    const auto source = impl_->propertyLookup(*ownerSource, observed);
    if (!source) return std::nullopt;
    const auto& variant = impl_->chooseVariant(
        impl_->properties[source->conceptIndex].variants, *source, targetModel);
    return Impl::iomName(variant.name, version, false);
}

std::vector<IomName> IlicModelIndex::transferProperties(
    const IomName& owner, std::string_view targetModel,
    XtfVersion version) const {
    const auto source = Impl::lookup(owner, impl_->classes, "class");
    if (!source) return {};
    const auto& classVariant = impl_->chooseVariant(
        impl_->classes[source->conceptIndex].variants, *source, targetModel);
    std::vector<IomName> result;
    result.reserve(classVariant.transferProperties.size());
    for (const auto propertyIndex : classVariant.transferProperties) {
        const Impl::Lookup propertySource{propertyIndex, std::nullopt};
        const auto& propertyVariant = impl_->chooseVariant(
            impl_->properties[propertyIndex].variants, propertySource,
            targetModel);
        result.push_back(
            Impl::iomName(propertyVariant.name, version, false));
    }
    return result;
}

std::optional<IomName> IlicModelIndex::referenceTargetClass(
    const IomName& owner, const IomName& property,
    std::string_view targetModel, XtfVersion version) const {
    const auto ownerSource = Impl::lookup(owner, impl_->classes, "class");
    if (!ownerSource) return std::nullopt;
    const auto propertySource = impl_->propertyLookup(*ownerSource, property);
    if (!propertySource) return std::nullopt;
    const auto& record = impl_->properties[propertySource->conceptIndex];
    if (!record.targetClass) return std::nullopt;
    const Impl::Lookup targetSource{*record.targetClass, std::nullopt};
    const auto& target = impl_->chooseVariant(
        impl_->classes[*record.targetClass].variants, targetSource,
        targetModel);
    return Impl::iomName(target.name, version, true);
}

std::optional<std::string> IlicModelIndex::translateEnumeration(
    const IomName& owner, const IomName& property,
    std::string_view lexicalValue, std::string_view targetModel) const {
    const auto ownerSource = Impl::lookup(owner, impl_->classes, "class");
    if (!ownerSource) return std::nullopt;
    const auto propertySource = impl_->propertyLookup(*ownerSource, property);
    if (!propertySource) return std::nullopt;
    const auto& record = impl_->properties[propertySource->conceptIndex];
    if (!record.hasEnumeration) return std::string(lexicalValue);
    const auto enumeration =
        impl_->enumConcept(record, *propertySource, lexicalValue);
    if (!enumeration) return std::nullopt;
    const auto& target = impl_->models[impl_->modelIndex(targetModel)];
    const Impl::EnumVariant* result = nullptr;
    for (const auto& variant : impl_->enumerations[*enumeration].variants) {
        if (variant.model == target.name) return variant.lexical;
        if (variant.language != target.language) continue;
        if (result != nullptr) {
            throw IoxError(DiagnosticCode::ModelMismatch,
                           "Ambiguous enumeration target language " +
                               target.language);
        }
        result = &variant;
    }
    if (result == nullptr) {
        throw IoxError(DiagnosticCode::ModelMismatch,
                       "Enumeration has no translation for target model " +
                           target.name);
    }
    return result->lexical;
}

bool IlicModelIndex::isTopLevelTransferable(
    const IomName& className) const {
    const auto source = Impl::lookup(className, impl_->classes, "class");
    return source && impl_->classes[source->conceptIndex].topLevelTransferable;
}

bool IlicModelIndex::isTransientProperty(
    const IomName& owner, const IomName& property) const {
    const auto ownerSource = Impl::lookup(owner, impl_->classes, "class");
    if (!ownerSource) return false;
    const auto source = impl_->propertyLookup(*ownerSource, property);
    return source && impl_->properties[source->conceptIndex].transient;
}

bool IlicModelIndex::isEmbeddedRole(
    const IomName& owner, const IomName& property) const {
    const auto ownerSource = Impl::lookup(owner, impl_->classes, "class");
    if (!ownerSource) return false;
    const auto source = impl_->propertyLookup(*ownerSource, property);
    return source && impl_->properties[source->conceptIndex].role &&
           impl_->properties[source->conceptIndex].embedded;
}

} // namespace ilic
} // namespace iox
