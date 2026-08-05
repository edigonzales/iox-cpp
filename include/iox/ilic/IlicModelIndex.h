#pragma once

// Direct integration with the concrete metamodel types supplied by
// ilic-core. This header is only available when IOX_ENABLE_ILIC is enabled.

#include "metamodel/MetaModelStore.h"
#include "iox/geometry/GeometryDescriptor.h"
#include "iox/Reader.h"
#include "iox/Writer.h"
#include "iox/Events.h"
#include "iox/xtf/XtfReaderOptions.h"

#include <memory>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace iox {
namespace ilic {

enum class PropertyKind {
    Attribute,
    Role
};

enum class PropertyValueKind {
    String,
    Boolean,
    Integer,
    Double,
    Structure,
    Reference,
    Geometry,
    Unknown
};

struct PropertyDescriptor final {
    IomName name;
    std::string propertyFqn;

    PropertyKind kind = PropertyKind::Attribute;
    PropertyValueKind valueKind = PropertyValueKind::Unknown;
    std::string interlisType;

    bool mandatory = false;
    std::int64_t cardinalityMin = 0;
    std::optional<std::int64_t> cardinalityMax;

    bool transient = false;
    bool embedded = false;

    std::optional<IomName> targetClass;
    std::optional<geometry::GeometryDescriptor> geometry;
};

class IlicModelIndex final {
public:
    /// Build a compact value index. The store is not retained and may be
    /// destroyed immediately after construction.
    explicit IlicModelIndex(const metamodel::MetaModelStore& models);
    ~IlicModelIndex();

    IlicModelIndex(const IlicModelIndex&) = delete;
    IlicModelIndex& operator=(const IlicModelIndex&) = delete;

    /// Resolve model metadata and translated transfer names. Unknown names
    /// return nullopt; ambiguous names throw IoxError(ModelMismatch).
    std::optional<std::string> modelLanguage(
        std::string_view modelName) const;
    std::optional<ModelEntry> transferModel(
        std::string_view modelName, XtfVersion version) const;

    std::optional<IomName> resolveTopic(
        const IomName& observed, std::string_view targetModel,
        XtfVersion version) const;
    std::optional<IomName> resolveClass(
        const IomName& observed, std::string_view targetModel,
        XtfVersion version) const;
    std::optional<IomName> resolveProperty(
        const IomName& owner, const IomName& observed,
        std::string_view targetModel, XtfVersion version) const;

    /// Return inherited attributes and roles in normative transfer order,
    /// excluding transient properties.
    std::vector<IomName> transferProperties(
        const IomName& owner, std::string_view targetModel,
        XtfVersion version) const;
    std::vector<PropertyDescriptor> transferPropertyDescriptors(
        const IomName& owner, std::string_view targetModel,
        XtfVersion version) const;
    std::optional<PropertyDescriptor> propertyDescriptor(
        const IomName& owner, const IomName& property,
        std::string_view targetModel, XtfVersion version) const;
    /// Returns the translated target class for role properties only.
    std::optional<IomName> referenceTargetClass(
        const IomName& owner, const IomName& property,
        std::string_view targetModel, XtfVersion version) const;
    /// Preserve non-enumeration lexemes and translate known enumeration paths,
    /// including OTHERS, into the selected target model.
    std::optional<std::string> translateEnumeration(
        const IomName& owner, const IomName& property,
        std::string_view lexicalValue, std::string_view targetModel) const;

    bool isTopLevelTransferable(const IomName& className) const;
    bool isTransientProperty(const IomName& owner,
                             const IomName& property) const;
    bool isEmbeddedRole(const IomName& owner,
                        const IomName& property) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct IlicXtfReaderOptions final {
    xtf::XtfReaderOptions xtf;
    bool rejectUnknownTopics = false;
    bool rejectUnknownClasses = false;
    bool rejectUnknownProperties = false;
};

/// Model-aware XTF reader composed around the generic model-free reader.
class IlicXtfReader final : public Reader {
public:
    IlicXtfReader(const metamodel::MetaModelStore& models,
                  IlicXtfReaderOptions options = {});
    ~IlicXtfReader() override;

    ReadOutcome next() override;
    void feed(ByteView data) override;
    void finish() override;
    bool isFinished() const noexcept override;
    std::vector<Diagnostic> takeDiagnostics() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct IlicXtfWriterOptions final {
    xtf::XtfWriterOptions xtf;
    bool enforceTransferOrder = true;
    bool rejectUnknownClasses = true;
    bool rejectUnknownProperties = true;
};

/// Model-aware XTF writer composed around the generic model-free writer.
class IlicXtfWriter final : public Writer {
public:
    IlicXtfWriter(const metamodel::MetaModelStore& models,
                  std::shared_ptr<OutputSink> output,
                  IlicXtfWriterOptions options = {});
    ~IlicXtfWriter() override;

    void write(const IoxEvent& event) override;
    void flush() override;
    void close() override;
    bool isClosed() const noexcept override;
    std::vector<Diagnostic> takeDiagnostics() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ilic
} // namespace iox
