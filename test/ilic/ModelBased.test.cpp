/// Model-aware transfer tests against the concrete ilic-core metamodel API.

#include "iox/ilic/IlicModelIndex.h"
#include "iox/test/Test.h"
#include "iox/Writer.h"
#include "iox/xtf/XtfWriter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

struct ModelFixture final {
    metamodel::MetaModelStore store;

    metamodel::Model* base = nullptr;
    metamodel::SubModel* data = nullptr;
    metamodel::Class* parent = nullptr;
    metamodel::Class* feature = nullptr;
    metamodel::Class* target = nullptr;
    metamodel::Class* detail = nullptr;
    metamodel::View* transientView = nullptr;
    metamodel::Class* link = nullptr;
    metamodel::AttrOrParam* code = nullptr;
    metamodel::AttrOrParam* name = nullptr;
    metamodel::AttrOrParam* status = nullptr;
    metamodel::AttrOrParam* details = nullptr;
    metamodel::AttrOrParam* calculated = nullptr;
    metamodel::Role* targetRole = nullptr;
    metamodel::EnumType* statusType = nullptr;
    metamodel::EnumNode* open = nullptr;
    metamodel::EnumNode* others = nullptr;

    metamodel::Model* french = nullptr;
    metamodel::SubModel* donnees = nullptr;
    metamodel::Class* parentFr = nullptr;
    metamodel::Class* featureFr = nullptr;
    metamodel::Class* targetFr = nullptr;
    metamodel::Class* detailFr = nullptr;
    metamodel::View* transientViewFr = nullptr;
    metamodel::Class* linkFr = nullptr;
    metamodel::AttrOrParam* codeFr = nullptr;
    metamodel::AttrOrParam* nameFr = nullptr;
    metamodel::AttrOrParam* statusFr = nullptr;
    metamodel::AttrOrParam* detailsFr = nullptr;
    metamodel::AttrOrParam* calculatedFr = nullptr;
    metamodel::Role* targetRoleFr = nullptr;
    metamodel::EnumType* statusTypeFr = nullptr;
    metamodel::EnumNode* openFr = nullptr;
    metamodel::EnumNode* othersFr = nullptr;
    metamodel::Model* otherModel = nullptr;

    ModelFixture() {
        buildBase();
        buildTranslation();
        addAmbiguousLocalClass();
        store.addModel(*base);
        store.addModel(*french);
        store.addModel(*otherModel);
    }

private:
    metamodel::Class* addClass(metamodel::SubModel& topic,
                               std::string nameValue, int kind) {
        auto* result = store.make<metamodel::Class>();
        result->Name = std::move(nameValue);
        result->Kind = static_cast<decltype(result->Kind)>(kind);
        result->ElementInPackage = &topic;
        topic.Element.push_back(result);
        return result;
    }

    metamodel::AttrOrParam* addAttribute(metamodel::Class& owner,
                                         std::string nameValue,
                                         metamodel::Type* type) {
        auto* result = store.make<metamodel::AttrOrParam>();
        result->Name = std::move(nameValue);
        result->AttrParent = &owner;
        result->Type = type;
        owner.ClassAttribute.push_back(result);
        return result;
    }

    metamodel::EnumNode* addEnumNode(metamodel::EnumNode& parent,
                                     std::string nameValue) {
        auto* result = store.make<metamodel::EnumNode>();
        result->Name = std::move(nameValue);
        result->ParentNode = &parent;
        parent.Node.push_back(result);
        return result;
    }

    void buildBase() {
        base = store.make<metamodel::Model>();
        base->Name = "BaseModel";
        base->Language = "de";
        base->Version = "1.0";
        base->At = "https://example.test/base";
        base->xmlns = "urn:example:base";

        data = store.make<metamodel::SubModel>();
        data->Name = "Data";
        data->ElementInPackage = base;
        base->Element.push_back(data);

        parent = addClass(*data, "Parent", metamodel::Class::ClassVal);
        feature = addClass(*data, "Feature", metamodel::Class::ClassVal);
        feature->Super = parent;
        target = addClass(*data, "Target", metamodel::Class::ClassVal);
        detail = addClass(*data, "Detail", metamodel::Class::Structure);

        transientView = store.make<metamodel::View>();
        transientView->Name = "CalculatedView";
        transientView->Kind = metamodel::Class::ViewVal;
        transientView->Transient = true;
        transientView->ElementInPackage = data;
        data->Element.push_back(transientView);

        auto* text = store.make<metamodel::TextType>();
        code = addAttribute(*parent, "Code", text);
        name = addAttribute(*feature, "Name", text);

        statusType = store.make<metamodel::EnumType>();
        auto* top = store.make<metamodel::EnumNode>();
        top->Name = "TOP";
        top->EnumType = statusType;
        statusType->TopNode = top;
        open = addEnumNode(*top, "Open");
        others = addEnumNode(*top, "OTHERS");
        status = addAttribute(*feature, "Status", statusType);
        details = addAttribute(*feature, "Details", detail);
        calculated = addAttribute(*feature, "Calculated", text);
        calculated->Transient = true;

        link = addClass(*data, "Link", metamodel::Class::Association);
        link->EmbeddedRoleTransfer = true;
        targetRole = store.make<metamodel::Role>();
        targetRole->Name = "TargetRole";
        targetRole->Association = link;
        targetRole->_baseclass = target;
        targetRole->EmbeddedTransfer = true;
        link->Role.push_back(targetRole);
        feature->_roleaccess.push_back(targetRole);
        auto* sourceRole = store.make<metamodel::Role>();
        sourceRole->Name = "SourceRole";
        sourceRole->Association = link;
        sourceRole->_baseclass = feature;
        link->Role.push_back(sourceRole);
    }

    void buildTranslation() {
        french = store.make<metamodel::Model>();
        french->Name = "Modele";
        french->Language = "fr";
        french->Version = "1.0-fr";
        french->At = "https://example.test/fr";
        french->_translationOf = base;

        donnees = store.make<metamodel::SubModel>();
        donnees->Name = "Donnees";
        donnees->_translationOf = data;
        donnees->ElementInPackage = french;
        french->Element.push_back(donnees);

        parentFr = addClass(*donnees, "ParentFr", metamodel::Class::ClassVal);
        parentFr->_translationOf = parent;
        featureFr = addClass(*donnees, "Objet", metamodel::Class::ClassVal);
        featureFr->_translationOf = feature;
        featureFr->Super = parentFr;
        targetFr = addClass(*donnees, "Cible", metamodel::Class::ClassVal);
        targetFr->_translationOf = target;
        detailFr = addClass(*donnees, "DetailFr", metamodel::Class::Structure);
        detailFr->_translationOf = detail;

        transientViewFr = store.make<metamodel::View>();
        transientViewFr->Name = "VueCalculee";
        transientViewFr->Kind = metamodel::Class::ViewVal;
        transientViewFr->Transient = true;
        transientViewFr->_translationOf = transientView;
        transientViewFr->ElementInPackage = donnees;
        donnees->Element.push_back(transientViewFr);

        auto* text = store.make<metamodel::TextType>();
        codeFr = addAttribute(*parentFr, "CodeFr", text);
        codeFr->_translationOf = code;
        nameFr = addAttribute(*featureFr, "Nom", text);
        nameFr->_translationOf = name;

        statusTypeFr = store.make<metamodel::EnumType>();
        statusTypeFr->_translationOf = statusType;
        auto* top = store.make<metamodel::EnumNode>();
        top->Name = "TOP";
        top->EnumType = statusTypeFr;
        top->_translationOf = statusType->TopNode;
        statusTypeFr->TopNode = top;
        openFr = addEnumNode(*top, "Ouvert");
        openFr->_translationOf = open;
        othersFr = addEnumNode(*top, "AUTRES");
        othersFr->_translationOf = others;
        statusFr = addAttribute(*featureFr, "Statut", statusTypeFr);
        statusFr->_translationOf = status;
        detailsFr = addAttribute(*featureFr, "DetailsFr", detailFr);
        detailsFr->_translationOf = details;
        calculatedFr = addAttribute(*featureFr, "Calcule", text);
        calculatedFr->_translationOf = calculated;
        calculatedFr->Transient = true;

        linkFr = addClass(*donnees, "Lien", metamodel::Class::Association);
        linkFr->_translationOf = link;
        linkFr->EmbeddedRoleTransfer = true;
        targetRoleFr = store.make<metamodel::Role>();
        targetRoleFr->Name = "RoleCible";
        targetRoleFr->_translationOf = targetRole;
        targetRoleFr->Association = linkFr;
        targetRoleFr->_baseclass = targetFr;
        targetRoleFr->EmbeddedTransfer = true;
        linkFr->Role.push_back(targetRoleFr);
        featureFr->_roleaccess.push_back(targetRoleFr);
        auto* sourceRole = store.make<metamodel::Role>();
        sourceRole->Name = "RoleSource";
        sourceRole->Association = linkFr;
        sourceRole->_baseclass = featureFr;
        linkFr->Role.push_back(sourceRole);
    }

    void addAmbiguousLocalClass() {
        otherModel = store.make<metamodel::Model>();
        otherModel->Name = "OtherModel";
        otherModel->Language = "en";
        otherModel->xmlns = "urn:example:other";
        auto* other = store.make<metamodel::SubModel>();
        other->Name = "Other";
        other->ElementInPackage = otherModel;
        otherModel->Element.push_back(other);
        (void)addClass(*other, "Feature", metamodel::Class::ClassVal);
    }
};

std::vector<iox::IoxEvent> transferEvents(iox::XtfVersion version) {
    iox::StartTransferEvent start;
    start.header.version = version;
    start.header.sender = "test";
    start.header.models.push_back({"Modele", {}, {}, {}});

    iox::StartBasketEvent basket;
    basket.basket.topic = iox::IomName("BaseModel.Data");
    basket.basket.basketId = "b1";

    iox::ObjectEvent object;
    object.object = iox::IomObject(
        iox::IomName("BaseModel.Data.Feature"), "o1");
    object.object.setPrimitive(iox::IomName("Status"), "Open");
    object.object.setPrimitive(iox::IomName("Name"), "nom");
    object.object.setPrimitive(iox::IomName("Code"), "001.2300");
    object.object.setObject(
        iox::IomName("Details"),
        iox::IomObject(iox::IomName("BaseModel.Data.Detail")));

    return {start, basket, object, iox::EndBasketEvent{},
            iox::EndTransferEvent{}};
}

std::string write(const ModelFixture& fixture, iox::XtfVersion version) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::ilic::IlicXtfWriterOptions options;
    options.xtf.version = version;
    options.xtf.pretty = false;
    iox::ilic::IlicXtfWriter writer(fixture.store, sink, options);
    for (const auto& event : transferEvents(version)) writer.write(event);
    writer.close();
    return sink->str();
}

} // namespace

IOX_TEST(model_index_copies_and_translates_names) {
    auto fixture = std::make_unique<ModelFixture>();
    iox::ilic::IlicModelIndex index(fixture->store);
    const auto topic = index.resolveTopic(
        iox::IomName("BaseModel.Data"), "Modele", iox::XtfVersion::V23);
    IOX_CHECK(topic.has_value());
    IOX_CHECK_EQ(std::string("Modele.Donnees"), topic->interlisName());

    const auto klass = index.resolveClass(
        iox::IomName("BaseModel.Data.Feature"), "Modele",
        iox::XtfVersion::V23);
    IOX_CHECK(klass.has_value());
    IOX_CHECK_EQ(std::string("Modele.Donnees.Objet"),
                 klass->interlisName());

    const auto property = index.resolveProperty(
        iox::IomName("BaseModel.Data.Feature"), iox::IomName("Name"),
        "Modele", iox::XtfVersion::V23);
    IOX_CHECK(property.has_value());
    IOX_CHECK_EQ(std::string("Nom"), property->interlisName());
}

IOX_TEST(model_index_does_not_retain_store_pointers) {
    std::unique_ptr<iox::ilic::IlicModelIndex> index;
    {
        ModelFixture fixture;
        index = std::make_unique<iox::ilic::IlicModelIndex>(fixture.store);
    }
    const auto klass = index->resolveClass(
        iox::IomName("BaseModel.Data.Feature"), "Modele",
        iox::XtfVersion::V23);
    IOX_CHECK(klass.has_value());
    IOX_CHECK_EQ(std::string("Modele.Donnees.Objet"),
                 klass->interlisName());
}

IOX_TEST(model_index_xtf24_uses_origin_wire_qnames) {
    ModelFixture fixture;
    iox::ilic::IlicModelIndex index(fixture.store);
    const auto klass = index.resolveClass(
        iox::IomName("Modele.Donnees.Objet"), "Modele",
        iox::XtfVersion::V24);
    IOX_CHECK(klass.has_value());
    IOX_CHECK_EQ(std::string("Modele.Donnees.Objet"),
                 klass->interlisName());
    IOX_CHECK_EQ(std::string("urn:example:base"),
                 klass->xmlName().namespaceUri);
    IOX_CHECK_EQ(std::string("Feature"), klass->xmlName().localName);
}

IOX_TEST(model_index_rejects_ambiguous_local_names) {
    ModelFixture fixture;
    iox::ilic::IlicModelIndex index(fixture.store);
    bool rejected = false;
    try {
        (void)index.resolveClass(iox::IomName("Feature"), "BaseModel",
                                 iox::XtfVersion::V23);
    } catch (const iox::IoxError& error) {
        rejected = error.code() == iox::DiagnosticCode::ModelMismatch;
    }
    IOX_CHECK(rejected);
    IOX_CHECK(index.resolveClass(iox::IomName("BaseModel.Data.Feature"),
                                 "BaseModel", iox::XtfVersion::V23)
                  .has_value());
}

IOX_TEST(model_index_transfer_order_skips_transient_properties) {
    ModelFixture fixture;
    iox::ilic::IlicModelIndex index(fixture.store);
    const auto order = index.transferProperties(
        iox::IomName("BaseModel.Data.Feature"), "Modele",
        iox::XtfVersion::V23);
    IOX_CHECK_EQ(static_cast<std::size_t>(5), order.size());
    IOX_CHECK_EQ(std::string("CodeFr"), order[0].interlisName());
    IOX_CHECK_EQ(std::string("Nom"), order[1].interlisName());
    IOX_CHECK_EQ(std::string("Statut"), order[2].interlisName());
    IOX_CHECK_EQ(std::string("DetailsFr"), order[3].interlisName());
    IOX_CHECK_EQ(std::string("RoleCible"), order[4].interlisName());
    IOX_CHECK(index.isTransientProperty(
        iox::IomName("BaseModel.Data.Feature"),
        iox::IomName("Calculated")));
}

IOX_TEST(model_index_translates_enumerations_including_others) {
    ModelFixture fixture;
    iox::ilic::IlicModelIndex index(fixture.store);
    const auto open = index.translateEnumeration(
        iox::IomName("BaseModel.Data.Feature"), iox::IomName("Status"),
        "Open", "Modele");
    const auto others = index.translateEnumeration(
        iox::IomName("BaseModel.Data.Feature"), iox::IomName("Status"),
        "OTHERS", "Modele");
    IOX_CHECK(open.has_value());
    IOX_CHECK(others.has_value());
    IOX_CHECK_EQ(std::string("Ouvert"), *open);
    IOX_CHECK_EQ(std::string("AUTRES"), *others);
}

IOX_TEST(model_index_exposes_roles_targets_views_and_structures) {
    ModelFixture fixture;
    iox::ilic::IlicModelIndex index(fixture.store);
    const auto target = index.referenceTargetClass(
        iox::IomName("BaseModel.Data.Feature"),
        iox::IomName("TargetRole"), "Modele", iox::XtfVersion::V23);
    IOX_CHECK(target.has_value());
    IOX_CHECK_EQ(std::string("Modele.Donnees.Cible"),
                 target->interlisName());
    IOX_CHECK(index.isEmbeddedRole(
        iox::IomName("BaseModel.Data.Feature"),
        iox::IomName("TargetRole")));
    IOX_CHECK(!index.isTopLevelTransferable(
        iox::IomName("BaseModel.Data.Detail")));
    IOX_CHECK(!index.isTopLevelTransferable(
        iox::IomName("BaseModel.Data.CalculatedView")));
    IOX_CHECK(index.isTopLevelTransferable(
        iox::IomName("BaseModel.Data.Link")));
}

IOX_TEST(model_writer_translates_and_orders_xtf23) {
    ModelFixture fixture;
    const auto xml = write(fixture, iox::XtfVersion::V23);
    IOX_CHECK(xml.find("<Modele.Donnees ") != std::string::npos);
    IOX_CHECK(xml.find("<Modele.Donnees.Objet") != std::string::npos);
    const auto code = xml.find("<CodeFr>001.2300</CodeFr>");
    const auto name = xml.find("<Nom>nom</Nom>");
    const auto status = xml.find("<Statut>Ouvert</Statut>");
    IOX_CHECK(code != std::string::npos);
    IOX_CHECK(code < name);
    IOX_CHECK(name < status);
    IOX_CHECK(xml.find("Modele.Donnees.DetailFr") != std::string::npos);
}

IOX_TEST(model_writer_and_reader_share_xtf24_mapping) {
    ModelFixture fixture;
    const auto xml = write(fixture, iox::XtfVersion::V24);
    IOX_CHECK(xml.find("xmlns:Modele=\"urn:example:base\"") !=
              std::string::npos);
    IOX_CHECK(xml.find("<Modele:Feature") != std::string::npos);
    IOX_CHECK(xml.find("<Modele:Name>nom</Modele:Name>") !=
              std::string::npos);

    iox::ilic::IlicXtfReaderOptions options;
    options.xtf.expectedVersion = iox::XtfVersion::V24;
    options.rejectUnknownClasses = true;
    options.rejectUnknownProperties = true;
    iox::ilic::IlicXtfReader reader(fixture.store, options);
    reader.feed(iox::ByteView(xml));
    reader.finish();

    bool foundObject = false;
    while (true) {
        auto outcome = reader.next();
        if (outcome.event) {
            if (const auto* object =
                    std::get_if<iox::ObjectEvent>(&*outcome.event)) {
                foundObject = true;
                IOX_CHECK_EQ(std::string("Modele.Donnees.Objet"),
                             object->object.tag().interlisName());
                IOX_CHECK_EQ(std::string("nom"),
                             std::string(*object->object.primitive("Nom")));
                IOX_CHECK_EQ(std::string("Ouvert"),
                             std::string(*object->object.primitive("Statut")));
            }
        }
        if (outcome.progress == iox::ReaderProgress::End) break;
    }
    IOX_CHECK(foundObject);
    for (const auto& diagnostic : reader.takeDiagnostics()) {
        IOX_CHECK(diagnostic.severity != iox::DiagnosticSeverity::Error);
    }
}

IOX_TEST(model_reader_preserves_unknown_names_with_diagnostics) {
    ModelFixture fixture;
    auto events = transferEvents(iox::XtfVersion::V23);
    auto& object = std::get<iox::ObjectEvent>(events[2]);
    object.object.setPrimitive(iox::IomName("Unknown"), "lexical");

    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions writerOptions;
    writerOptions.version = iox::XtfVersion::V23;
    writerOptions.pretty = false;
    iox::xtf::XtfWriter generic(sink, writerOptions);
    auto& header = std::get<iox::StartTransferEvent>(events[0]).header;
    header.models[0] = {"Modele", "1.0-fr",
                        "https://example.test/fr", {}};
    for (const auto& event : events) generic.write(event);
    generic.close();

    iox::ilic::IlicXtfReader reader(fixture.store);
    reader.feed(iox::ByteView(sink->str()));
    reader.finish();
    bool preserved = false;
    while (true) {
        auto outcome = reader.next();
        if (outcome.event) {
            if (const auto* read =
                    std::get_if<iox::ObjectEvent>(&*outcome.event)) {
                preserved = read->object.primitive("Unknown").has_value();
            }
        }
        if (outcome.progress == iox::ReaderProgress::End) break;
    }
    IOX_CHECK(preserved);
    bool diagnosed = false;
    for (const auto& diagnostic : reader.takeDiagnostics()) {
        if (diagnostic.code == iox::DiagnosticCode::UnknownInterlisName) {
            diagnosed = true;
        }
    }
    IOX_CHECK(diagnosed);
}

IOX_TEST(model_writer_failure_is_terminal) {
    ModelFixture fixture;
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::ilic::IlicXtfWriterOptions options;
    options.xtf.version = iox::XtfVersion::V23;
    iox::ilic::IlicXtfWriter writer(fixture.store, sink, options);
    auto events = transferEvents(iox::XtfVersion::V23);
    writer.write(events[0]);
    writer.write(events[1]);
    auto& object = std::get<iox::ObjectEvent>(events[2]);
    object.object.setTag(iox::IomName("BaseModel.Data.Missing"));
    bool rejected = false;
    try {
        writer.write(object);
    } catch (const iox::IoxError& error) {
        rejected = error.code() == iox::DiagnosticCode::UnknownInterlisName;
    }
    IOX_CHECK(rejected);
    bool terminal = false;
    try {
        writer.write(iox::EndBasketEvent{});
    } catch (const iox::IoxError& error) {
        terminal = error.code() == iox::DiagnosticCode::WriterStateError;
    }
    IOX_CHECK(terminal);
}

IOX_TEST(model_index_negative_lookups_are_explicit) {
    ModelFixture fixture;
    iox::ilic::IlicModelIndex index(fixture.store);

    IOX_CHECK(!index.modelLanguage("Missing"));
    IOX_CHECK(!index.transferModel("Missing", iox::XtfVersion::V23));
    const auto base23 = index.transferModel("BaseModel", iox::XtfVersion::V23);
    const auto base24 = index.transferModel("BaseModel", iox::XtfVersion::V24);
    IOX_CHECK(base23.has_value());
    IOX_CHECK(base23->version.has_value());
    IOX_CHECK(base23->uri.has_value());
    IOX_CHECK(base24.has_value());
    IOX_CHECK(base24->xmlNamespace.namespaceUri == "urn:example:base");

    const iox::IomName missing("Missing");
    const iox::IomName feature("BaseModel.Data.Feature");
    IOX_CHECK(!index.resolveTopic(missing, "BaseModel", iox::XtfVersion::V23));
    IOX_CHECK(!index.resolveClass(missing, "BaseModel", iox::XtfVersion::V23));
    IOX_CHECK(!index.resolveProperty(missing, iox::IomName("Name"),
                                     "BaseModel", iox::XtfVersion::V23));
    IOX_CHECK(!index.resolveProperty(feature, missing, "BaseModel",
                                     iox::XtfVersion::V23));
    IOX_CHECK(index.transferProperties(missing, "BaseModel",
                                       iox::XtfVersion::V23).empty());
    IOX_CHECK(!index.referenceTargetClass(
        missing, iox::IomName("TargetRole"), "BaseModel",
        iox::XtfVersion::V23));
    IOX_CHECK(!index.referenceTargetClass(
        feature, missing, "BaseModel", iox::XtfVersion::V23));
    IOX_CHECK(!index.referenceTargetClass(
        feature, iox::IomName("Name"), "BaseModel",
        iox::XtfVersion::V23));

    IOX_CHECK(!index.translateEnumeration(
        missing, iox::IomName("Status"), "Open", "BaseModel"));
    IOX_CHECK(!index.translateEnumeration(
        feature, missing, "Open", "BaseModel"));
    const auto ordinary = index.translateEnumeration(
        feature, iox::IomName("Name"), "literal", "BaseModel");
    IOX_CHECK(ordinary.has_value());
    IOX_CHECK_EQ(std::string("literal"), *ordinary);
    IOX_CHECK(!index.translateEnumeration(
        feature, iox::IomName("Status"), "Missing", "BaseModel"));

    IOX_CHECK(!index.isTopLevelTransferable(missing));
    IOX_CHECK(!index.isTransientProperty(missing, iox::IomName("Name")));
    IOX_CHECK(!index.isTransientProperty(feature, missing));
    IOX_CHECK(!index.isTransientProperty(feature, iox::IomName("Name")));
    IOX_CHECK(!index.isEmbeddedRole(missing, iox::IomName("TargetRole")));
    IOX_CHECK(!index.isEmbeddedRole(feature, missing));
    IOX_CHECK(!index.isEmbeddedRole(feature, iox::IomName("Name")));

    bool targetRejected = false;
    try {
        (void)index.resolveClass(feature, "Missing", iox::XtfVersion::V23);
    } catch (const iox::IoxError& error) {
        targetRejected = error.code() == iox::DiagnosticCode::ModelMismatch;
    }
    IOX_CHECK(targetRejected);
}

IOX_TEST(model_transformer_reports_topics_classes_properties_and_enums) {
    ModelFixture fixture;
    auto makeWriter = [&](iox::ilic::IlicXtfWriterOptions options) {
        return std::make_unique<iox::ilic::IlicXtfWriter>(
            fixture.store, std::make_shared<iox::StringOutputSink>(), options);
    };

    iox::ilic::IlicXtfWriterOptions options;
    options.xtf.version = iox::XtfVersion::V23;
    options.xtf.pretty = false;

    auto mixed = makeWriter(options);
    auto mixedStart = std::get<iox::StartTransferEvent>(
        transferEvents(iox::XtfVersion::V23)[0]);
    mixedStart.header.models.push_back({"BaseModel", {}, {}, {}});
    bool languageRejected = false;
    try { mixed->write(mixedStart); }
    catch (const iox::IoxError& error) {
        languageRejected = error.code() == iox::DiagnosticCode::ModelMismatch;
    }
    IOX_CHECK(languageRejected);

    auto unknownTopic = makeWriter(options);
    auto events = transferEvents(iox::XtfVersion::V23);
    unknownTopic->write(events[0]);
    auto badBasket = std::get<iox::StartBasketEvent>(events[1]);
    badBasket.basket.topic = iox::IomName("Missing.Topic");
    bool topicRejected = false;
    try { unknownTopic->write(badBasket); }
    catch (const iox::IoxError& error) {
        topicRejected = error.code() == iox::DiagnosticCode::UnknownInterlisName;
    }
    IOX_CHECK(topicRejected);

    auto unknownProperty = makeWriter(options);
    unknownProperty->write(events[0]);
    unknownProperty->write(events[1]);
    auto badObject = std::get<iox::ObjectEvent>(events[2]);
    badObject.object.setPrimitive(iox::IomName("Missing"), "x");
    bool propertyRejected = false;
    try { unknownProperty->write(badObject); }
    catch (const iox::IoxError& error) {
        propertyRejected = error.code() == iox::DiagnosticCode::UnknownInterlisName;
    }
    IOX_CHECK(propertyRejected);

    auto transientProperty = makeWriter(options);
    transientProperty->write(events[0]);
    transientProperty->write(events[1]);
    badObject = std::get<iox::ObjectEvent>(events[2]);
    badObject.object.setPrimitive(iox::IomName("Calculated"), "x");
    bool transientRejected = false;
    try { transientProperty->write(badObject); }
    catch (const iox::IoxError& error) {
        transientRejected = error.code() == iox::DiagnosticCode::UnknownInterlisName;
    }
    IOX_CHECK(transientRejected);

    auto badEnumeration = makeWriter(options);
    badEnumeration->write(events[0]);
    badEnumeration->write(events[1]);
    badObject = std::get<iox::ObjectEvent>(events[2]);
    badObject.object.replaceValue(
        "Status", 0, iox::IomValue::primitive("Missing"));
    bool enumRejected = false;
    try { badEnumeration->write(badObject); }
    catch (const iox::IoxError& error) {
        enumRejected = error.code() == iox::DiagnosticCode::UnknownInterlisName;
    }
    IOX_CHECK(enumRejected);

    auto structure = makeWriter(options);
    structure->write(events[0]);
    structure->write(events[1]);
    badObject.object = iox::IomObject(
        iox::IomName("BaseModel.Data.Detail"), "D");
    bool structureRejected = false;
    try { structure->write(badObject); }
    catch (const iox::IoxError& error) {
        structureRejected = error.code() == iox::DiagnosticCode::UnknownInterlisName;
    }
    IOX_CHECK(structureRejected);
}

IOX_TEST(model_reader_and_writer_wrapper_states_are_observable) {
    ModelFixture fixture;
    const auto xml = write(fixture, iox::XtfVersion::V23);
    iox::ilic::IlicXtfReader reader(fixture.store);
    IOX_CHECK(!reader.isFinished());
    reader.feed(iox::ByteView(xml));
    reader.finish();
    while (reader.next().progress == iox::ReaderProgress::Event) {}
    IOX_CHECK(reader.isFinished());
    (void)reader.takeDiagnostics();

    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::ilic::IlicXtfWriterOptions options;
    options.xtf.version = iox::XtfVersion::V23;
    options.xtf.pretty = false;
    iox::ilic::IlicXtfWriter writer(fixture.store, sink, options);
    IOX_CHECK(!writer.isClosed());
    auto events = transferEvents(iox::XtfVersion::V23);
    writer.write(events[0]);
    writer.flush();
    for (std::size_t index = 1; index < events.size(); ++index) {
        writer.write(events[index]);
    }
    writer.close();
    IOX_CHECK(writer.isClosed());
    (void)writer.takeDiagnostics();

    auto failedSink = std::make_shared<iox::StringOutputSink>();
    iox::ilic::IlicXtfWriter failed(fixture.store, failedSink, options);
    bool flushRejected = false;
    try { failed.flush(); }
    catch (const iox::IoxError& error) {
        flushRejected = error.code() == iox::DiagnosticCode::WriterStateError;
    }
    IOX_CHECK(flushRejected);
    IOX_CHECK(!failed.isClosed());
    bool terminal = false;
    try { failed.close(); }
    catch (const iox::IoxError& error) {
        terminal = error.code() == iox::DiagnosticCode::WriterStateError;
    }
    IOX_CHECK(terminal);

    auto incompleteSink = std::make_shared<iox::StringOutputSink>();
    iox::ilic::IlicXtfWriter incomplete(fixture.store, incompleteSink, options);
    incomplete.write(events[0]);
    bool closeRejected = false;
    try { incomplete.close(); }
    catch (const iox::IoxError& error) {
        closeRejected = error.code() == iox::DiagnosticCode::WriterStateError;
    }
    IOX_CHECK(closeRejected);
}

#include "iox/test/TestMain.h"
