# iox-ili XTF Test Porting Matrix

This document is the method-level inventory for the pinned `iox-ili` XTF
tests. It records the behavioral mapping to the C++ event API; it is not a
claim that Java source can be copied line-for-line.

## Reference and counting rules

- Repository: `https://github.com/claeis/iox-ili`
- Immutable source revision: `1af01d4bf6b675a490b9f5ad44d41723fdfa3c0f`
- License of the referenced test data: MIT/X License
- Relevant Java XTF methods inventoried: 207
- Relevant XTF factory/utility methods inventoried: 7
- Checked-in XTF transfer fixtures: 211 (`94` XTF 2.3, `112` XTF 2.4,
  `5` XTF 2.4 writer)
- Checked-in model-support fixtures: 9 `.ili` files
- Fixture provenance manifest: `test/fixtures/iox-ili-fixtures.tsv`
- Offline manifest check: `./scripts/verify-iox-ili-fixtures.sh`

Each method listed in a row has the status in that row. A method can be
covered by an adapted generic test while its model-specific Java assertion
remains an explicit `api-gap`; that distinction is recorded in the notes.

### Status definitions

| Status | Meaning |
|---|---|
| `ported` | The behavior and assertion are directly represented in C++. |
| `adapted` | The behavior is represented, but assertions use the ordered C++ event stream, COW IOM objects, structured diagnostics, or canonical XML. |
| `ilic-required` | The assertion requires concrete model metadata and belongs in the optional `iox-ilic` build. |
| `out-of-scope` | The Java test covers ITF, CSV, GML, JTS, or another excluded product area. |
| `api-gap` | The behavior is relevant but the current public C++ API or direct `ilic-core` adapter does not expose the required capability. |

## XTF 2.3 reader

| Java source | Java test methods | Status | C++ verification |
|---|---|---|---|
| `Xtf23ReaderTest.java` | `transferElement_Ok`, `testTextBetweenLines_Fail`, `testXML1Line_Ok`, `test_XTF22_Ok`, `test_WrongSpelledEndTransferElement_False`, `test_WrongSpelledStartTransferElement_False`, `test_CompleteOtherSpelledStartTransferElement_False`, `test_WrongSpelledStartAndEndTransferElement_False`, `test_WrongCaseSensitiveTransferElement_False`, `test_InvalidFormatOfTransferElement_False`, `test_DeleteObject_Ok`, `test_Consistency_Ok`, `test_DeleteObjectNoTid_Fail`, `test_StartEndState_Ok`, `test_BasketWithTransferKind_Ok`, `test_BasketWithDomains_Ok`, `test_BasketWithConsistency_Ok`, `test_ObjectOperationMode_Ok`, `test_ObjectWithBID_Ok` | `adapted` | `iox_ili_fixture_matrix_preserves_event_and_diagnostic_streams`; existing XTF 2.3 state/conformance tests |
| `Xtf23ReaderHeaderTest.java` | `test_ValidHeaderSection_Ok`, `test_AliasComment_Ok`, `test_EmptyAlias22_Ok`, `test_CommentsInFile_Ok`, `test_MultipleMODELDefined_Ok`, `test_DataSectionInsideHeaderSection_Fail`, `test_DataBeforeHeaderSection_Fail`, `test_MultipleHeaderSection_Fail`, `test_MultipleMODELSDefined_Fail`, `test_HeaderWrongTypeInsideModels_Fail`, `test_HeaderNoModelInsideModelsDefined_Fail`, `test_NoModelInsideModelDefined_Fail`, `test_NoModelnameFound_Fail`, `test_HeaderSectionWithoutVersionStrict_Fail`, `test_HeaderSectionWithoutSender_Fail`, `test_HeaderSectionWithoutSenderAndVersion_Fail`, `test_HeaderSectionModelWithoutName_Fail`, `test_HeaderSectionModelWithoutVersion_Ok`, `test_HeaderSectionModelWithoutUriStrict_Fail`, `test_HeaderSectionModelWithoutUri_Ok`, `test_HeaderSectionModelWithoutNameVersionUri_Fail` | `adapted` | Existing `iox.test.conformance`, strict reader options, and all-fixture chunk matrix |
| `Xtf23ReaderDataTest.java` | `testDatasection_Empty_Ok`, `testBasket_Empty_Ok`, `testMultiBasket_Ok`, `testEmptyObjects_Ok`, `testBooleanDataTypes_Ok`, `testTextType_Ok`, `testTextType_List_Ok`, `testTextTypes_WithEmptyLine_Ok`, `testEnumerationType_Ok`, `testOidType_Ok`, `testOidType_Fail`, `testDateAndTimeType_Ok`, `testBlackBoxType_Ok`, `testBlackBoxType_NoSpace_Ok`, `testNumericDataTypes_Ok`, `testAlignmentDataTypes_Ok`, `testFormattedDataTypes_Ok`, `testStructureType_Ok`, `testStructure2Type_Ok`, `testReferenceAttrType_Ok`, `testAttributePath_Ok`, `testCoords_Ok`, `testPolylinesWithStraights_Ok`, `testPolylinesWithArcs_Ok`, `testPolylinesWithArcsNoSpace_Ok`, `testPolylinesWithArcsRadius_Ok`, `testPolylineNoSegment_Fail`, `testSurface_Ok`, `testCommentary_Ok`, `testArea_Ok`, `testView_Ok`, `testSurfaceNoBoundary_Fail`, `testSurfaceNoBoundary_NoSpace_Fail`, `testSurfaceNoPolyline_Fail`, `testMissingCoord_Fail`, `testSameAttrNamesInDifClasses_Ok`, `testSameClassNamesInDifTopics_Ok`, `testTopicNameLikeClassName_Ok`, `testAttrClassTopicNameSame_Ok` | `adapted` | Existing XTF 2.3 object/geometry tests plus all-fixture event fingerprints |
| `Xtf23ReaderAssociationTest.java` | `embedded_Ok`, `embeddedAssociationWithAttributes_Ok`, `embedded_ClassPathRef_Ok`, `standAlone_WithAttributes_Ok`, `setOrderPos_Ok`, `embedded_1to1_OrderPos_Ok`, `standAlone_Ok`, `commentsInsideAssociation_Ok`, `sameTargetClass_Ok`, `deleteObjectWithRef_Fail` | `adapted` | Existing association tests; `iox_ili_xtf23_reference_fixture_preserves_attributes_and_values` |

## XTF 2.4 reader

| Java source | Java test methods | Status | C++ verification |
|---|---|---|---|
| `Xtf24ReaderTest.java` | `testTransfer_Ok`, `testTransferNoSpace_Ok`, `testTextBetweenLines_Fail`, `testXML1Line_Ok`, `testComments_Ok`, `testEmptyBasket_Ok`, `testMultipleBaskets_Ok`, `testMultipleBasketsNoSpace_Ok`, `testEmptyObjects_Ok`, `testDeleteObject_Ok`, `testDeleteObjectNoTid_Fail`, `testStartEndState_Ok`, `testMultipleBasketsAndObjects_Ok`, `testMultipleBasketsAndObjectsNoSpace_Ok`, `testBasketWithTransferKind_Ok`, `testBasketWithDomains_Ok`, `testBasketWithConsistency_Ok`, `testObjectOperationMode_Ok`, `testNoDataSectionDefined_Fail`, `testMultipleDataSectionsDefined_Fail`, `testWrongBasketId_Fail`, `testWrongObjectId_Fail`, `testWrongTopEleNamespace_Fail`, `testUnexpectedCharacter_Fail`, `testUnexpectedEvent_Fail`, `testWrongTopEleName_Fail` | `adapted` | `iox_ili_fixture_matrix_preserves_event_and_diagnostic_streams`; XTF 2.4 namespace and state tests |
| `Xtf24ReaderTest.java` | `testSkipBasket_Ok`, `testSkipBasketFirst_Ok`, `testSkipBasketOnly_Ok` | `api-gap` | Requires a model-backed basket filter; the generic reader intentionally preserves the stream instead of filtering it |
| `Xtf24ReaderHeaderTest.java` | `testCommentsInFile_Ok`, `testHeaderComments_Ok`, `testHeaderSender_Ok`, `testHeaderSenderAndComments_Ok`, `xml1Line_Ok`, `testHeaderCommentsBeforeSender_Ok`, `testDataSectionInsideHeaderSection_Fail`, `testDataBeforeHeaderSection_Fail`, `test_MultipleHeaderSection_Fail`, `test_MultipleSender_Fail`, `test_MultipleComments_Fail`, `testHeaderSenderBeforeModels_Fail`, `testHeaderCommentsBeforeModels_Fail`, `testNoSendernameFound_Fail`, `testNoCommentsnameFound_Fail` | `adapted` | Header event/diagnostic fingerprinting and chunk matrix |
| `Xtf24ReaderHeaderTest.java` | `testHeaderMultipleModelDefined_Ok`, `testNoModelsDefined_Fail`, `testMultipleModels_Fail`, `testHeaderWrongTypeInsideModels_Fail`, `testHeaderNoModelInsideModelsDefined_Fail`, `testNoModelInsideModelDefined_Fail`, `testWrongTypeInModel_Fail`, `testNoModelnameFound_Fail` | `api-gap` | `StartTransferEvent` does not yet expose an ordered model declaration list; representative model checks are in `test/ilic/ModelBased.test.cpp` |
| `Xtf24ReaderDataTest.java` | `testTextType_Ok`, `testTextType_List_Ok`, `xml1Line_Ok`, `testSameAttrNamesInDifClasses_Ok`, `testSameClassNamesInDifTopics_Ok`, `testTopicNameLikeClassName_Ok`, `testAttrClassTopicNameSame_Ok`, `testEnumerationType_Ok`, `testOidType_Ok`, `testDateAndTimeType_Ok`, `testBlackBoxType_Ok`, `testNumericDataTypes_Ok`, `testBooleanDataTypes_Ok`, `testAlignmentDataTypes_Ok`, `testFormattedDataTypes_Ok`, `testStructureType_Ok`, `testAttributePath_Ok`, `testCoord_Ok`, `testCoordNoSpace_Ok`, `testPolylinesWithStraights_Ok`, `testPolylinesWithArcs_Ok`, `testPolylinesWithArcsNoSpace_Ok`, `testPolylinesWithArcsRadius_Ok`, `testMultiPolyline_Ok`, `testSurface_Ok`, `testCommentary_Ok`, `testArea_Ok`, `testMultiSurface_Ok`, `testMultiArea_Ok`, `testView_Ok`, `testUnsupportedGeometry_Fail`, `testSurfaceNoLinesFound_Fail`, `testMissingCoord_Fail`, `testUnexpectedCharacters_Fail`, `testReferenceAttribute_List_Ok` | `adapted` | XTF 2.4 geometry tests, fixture fingerprints, and unknown geometry preservation |
| `Xtf24ReaderDataTest.java` | `testViewNotOfTopicView_Fail`, `testEnumerationOthers_ok`, `testSubEnumerationOthers_ok`, `testViewIsTransient_Fail` | `api-gap` | Requires model type, view, or enumeration semantics not exposed by the generic core |
| `Xtf24ReaderAssociationTest.java` | `embeddedAssociationWithAttributes_Ok`, `embedded_0to1_Ok`, `embedded_0to0_Ok`, `embedded_1to1_Ok`, `embedded_1toN_Ok`, `embedded_Nto1_Ok`, `embedded2_1to1_Ok`, `embedded_1to1_OrderPos_Ok`, `alone_NtoN_Ok`, `xml1Line_Ok`, `alone_WithAttributes_Ok`, `commentsInsideAssociation_Ok`, `sameTargetClass_Ok`, `testDeleteObjectWithRef_Fail`, `valid_0to0Association_Ok` | `adapted` | Association event/reference preservation and ordered attribute tests |
| `Xtf24ReaderAssociationTest.java` | `noAssociationName_Ok`, `moreRolesThanDefined_Ok`, `roleNotExist_Fail`, `associationNotExist_Fail` | `api-gap` | Requires model role/cardinality and association-name validation |

## XTF 2.4 writer

| Java source | Java test methods | Status | C++ verification |
|---|---|---|---|
| `Xtf24WriterTest.java` | `writePolylineObjectEvent`, `writeMultiCoordObjectEvent`, `writeReferenceAttr`, `writeMultiPolylineObjectEvent`, `writeMultiSurfaceObjectEvent` | `adapted` | `iox_ili_xtf24_writer_fixtures_have_semantic_roundtrip`; deterministic generic writer test |
| `Xtf24ReaderTranslationTest.java` | `TranslatedModelName_Ok` | `api-gap` | Requires translated model-name mapping |
| `Xtf24WriterTranslationTest.java` | `writeBasket_Ok`, `writeTranslatedBasket_Fail` | `api-gap` | Requires translated model mapping and model-aware writer validation |

## Factory and utility tests

| Java source | Java test methods | Status | C++ verification |
|---|---|---|---|
| `ReaderFactoryTest.java` | `xtf23Reader_Ok`, `xtf23Reader_txtExtension_Ok`, `xtf24Reader_Ok`, `xtf24Reader_txtExtension_Ok` | `adapted` | `factory_xtf_extensions_select_xtf_reader`, existing factory sniffing tests |
| `GetModelsTest.java` | `xtf23Reader_Ok`, `xtf23ReaderNoModels_Ok`, `xtf24Reader_Ok` | `api-gap` | No public C++ model-list accessor exists; model declarations must first be added to the normative event/header API |
| `ReaderFactoryTest.java` | `itfReader2_Ok`, `itfReader2_txtExtension_Ok`, `itfReader2_csvExtension_Ok`, `csvReader_itfExtension_fail`, `csvReader_EmptyCsvFile_Ok`, `csvReader_Ok`, `csvReader_txtExtension_fail`, `gml20Reader_ili10_Ok`, `gml20Reader_ili10_csvFile_Ok`, `gml20Reader_ili10_txtExtension_Ok`, `gml20Reader_ili23_Ok`, `gml20Reader_ili23_txtExtension_Ok` | `out-of-scope` | ITF, CSV, and GML are excluded by the product specification |
| `GetModelsTest.java` | `itfReader2_Ok`, `csvReader_Ok`, `gml20Reader_Ok` | `out-of-scope` | ITF, CSV, and GML are excluded by the product specification |

## C++ verification contract

The ported tests use `test/conformance/IoxIliTestSupport.h` to compare:

- ordered `IoxEvent` variants;
- object tags, object identity, ordered attributes, repeated values, and
  reference metadata;
- expanded XML names where XTF 2.4 provides them;
- diagnostic severity and stable diagnostic code;
- one-shot parsing against one-byte, seven-byte, and 64-byte chunking;
- semantic Reader → Writer → Reader roundtrips; and
- deterministic writer bytes for the same event stream.

Java exception text and Java-specific XML lexical choices are not compared
byte-for-byte. XTF 2.3 default-namespace/header-attribute differences and
the C++ canonical `ili:` representation remain documented in
`docs/conformance.md`.

## Remaining work represented by the matrix

The matrix makes the remaining model-aware/API work explicit. The generic
core does not invent a model-provider abstraction and does not reject or
filter fachlich relevant data merely because it lacks a model. Future
`iox-ilic` work can move individual `api-gap` rows to `ilic-required` when a
concrete `ilic-core` model capability and a regression test exist.
