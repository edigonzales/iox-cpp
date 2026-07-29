#pragma once

// ============================================================================
// Minimal INTERLIS metamodel types for model-aware XTF processing.
// ============================================================================
//
// These types represent the subset of INTERLIS model information needed
// for model-aware XTF reading/writing:
//   - Topic/class/property names
//   - Attribute types (text, numeric, structure, reference)
//   - Transfer order (attribute sequence)
//   - XML namespace mapping for XTF 2.4
//
// When ilic-fork is available, these can be populated from its
// semantic output. For now, they are constructed manually in tests.

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <optional>
#include <memory>
#include <utility>

namespace iox {
namespace ilic {

// ============================================================================
// Property types
// ============================================================================

enum class PropertyType {
    Text,
    Integer,
    Decimal,
    Boolean,
    Enumeration,
    Date,
    DateTime,
    Time,
    BlackBox,
    Structure,    // nested IomObject
    Reference     // REF to another object
};

/// A property (attribute or role) of a class.
struct PropertyDef final {
    std::string name;               // INTERLIS attribute name
    PropertyType type = PropertyType::Text;
    std::string targetClass;        // For Structure and Reference types
    bool optional = false;
    int orderPos = 0;               // Transfer order position

    // XTF 2.4 namespace mapping
    std::optional<std::string> xmlNamespace;
    std::optional<std::string> xmlLocalName;
};

// ============================================================================
// Class definition
// ============================================================================

struct ClassDef final {
    std::string name;               // INTERLIS class name (scoped: MODEL.TOPIC.Class)
    std::string topicName;          // Owning topic
    std::vector<PropertyDef> properties; // In transfer order
    bool isAssociation = false;

    const PropertyDef* findProperty(std::string_view name) const {
        for (auto& p : properties) {
            if (p.name == name) return &p;
        }
        return nullptr;
    }
};

// ============================================================================
// Topic definition
// ============================================================================

struct TopicDef final {
    std::string name;               // INTERLIS topic name (scoped: MODEL.TOPIC)
    std::vector<std::string> classNames; // Class names in this topic
};

// ============================================================================
// Model definition
// ============================================================================

/// A complete INTERLIS transfer model description.
///
/// Contains topics, classes, and their properties. Used by
/// IlicModelIndex for model-aware XTF processing.
struct ModelDef final {
    std::string name;
    std::string version;
    std::string uri;

    std::vector<TopicDef> topics;
    std::map<std::string, ClassDef> classes; // keyed by scoped class name

    const TopicDef* findTopic(std::string_view name) const {
        for (auto& t : topics) {
            if (t.name == name) return &t;
        }
        return nullptr;
    }

    const ClassDef* findClass(std::string_view scopedName) const {
        auto it = classes.find(std::string(scopedName));
        return it != classes.end() ? &it->second : nullptr;
    }

    // --- Builder helpers ---

    void addTopic(std::string name) {
        topics.push_back({std::move(name), {}});
    }

    void addClass(std::string scopedName, std::string topicName,
                  std::vector<PropertyDef> properties = {},
                  bool isAssociation = false) {
        ClassDef cls;
        cls.name = scopedName;
        cls.topicName = topicName;
        cls.properties = std::move(properties);
        cls.isAssociation = isAssociation;
        classes[scopedName] = std::move(cls);

        // Add to topic
        for (auto& t : topics) {
            if (t.name == topicName) {
                t.classNames.push_back(scopedName);
                break;
            }
        }
    }

    static ModelDef createTestModel() {
        ModelDef m;
        m.name = "TestModel";
        m.version = "2025-01-01";
        m.uri = "http://test.interlis.ch";

        m.addTopic("TestModel.TopicA");

        m.addClass("TestModel.TopicA.ClassA", "TestModel.TopicA", {
            {"Name",      PropertyType::Text,      "", false, 1},
            {"Count",     PropertyType::Integer,   "", false, 2},
            {"IsActive",  PropertyType::Boolean,   "", true,  3},
            {"Value",     PropertyType::Decimal,   "", true,  4},
        });

        m.addClass("TestModel.TopicA.ClassB", "TestModel.TopicA", {
            {"Description", PropertyType::Text,    "", false, 1},
            {"RefToA",     PropertyType::Reference, "TestModel.TopicA.ClassA", false, 2},
        });

        return m;
    }
};

} // namespace ilic
} // namespace iox
