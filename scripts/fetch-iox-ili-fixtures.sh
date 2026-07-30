#!/usr/bin/env bash
# fetch-iox-ili-fixtures.sh
# Downloads the selected XTF test fixtures from an immutable iox-ili revision.
# The checked-in complete transfer corpus is documented by the fixture manifest.
set -euo pipefail

IOX_ILI_COMMIT="1af01d4bf6b675a490b9f5ad44d41723fdfa3c0f"
BASE_URL="https://raw.githubusercontent.com/claeis/iox-ili/${IOX_ILI_COMMIT}/src/test/data"
FIXTURE_DIR="test/fixtures"

echo "=== Fetching iox-ili test fixtures ==="

# ---- Xtf23Reader root ----
ROOT="$BASE_URL/Xtf23Reader"
FILES=(
    BasketWithConsistency.xtf BasketWithTopics.xtf BasketWithTransferKind.xtf
    CompleteOtherSpelledStartTransferElement.xtf DeleteObject.xtf
    DeleteObjectNoTid.xtf InvalidFormatOfTransferElement.xtf
    ObjectConsistencyMode.xtf ObjectOperationMode.xtf ObjectWithBID.xtf
    Simple22a.xtf StartAndEndState.xtf TextBetweenLines.xtf
    ValidTransferElement.xtf WrongCaseSensitiveTransferElement.xtf
    WrongSpelledEndTransferElement.xtf WrongSpelledStartAndEndTransferElement.xtf
    WrongSpelledStartTransferElement.xtf Xml1Line.xtf
)
for f in "${FILES[@]}"; do
    curl -sL "$ROOT/$f" -o "$FIXTURE_DIR/xtf23/$f" 2>/dev/null || true
done

# ---- Xtf23Reader/headerSection ----
D="$BASE_URL/Xtf23Reader/headerSection"
mkdir -p "$FIXTURE_DIR/xtf23/headerSection"
FILES=(
    AliasComment.xtf CommentsInFile.xtf DataBeforeHeaderSection.xtf
    DataInsideHeaderSection.xtf EmptyAlias22.xtf FileWithoutHeaderSection.xtf
    HeaderSectionModelWithoutName.xtf HeaderSectionModelWithoutNameVersionUri.xtf
    HeaderSectionModelWithoutUri.xtf HeaderSectionModelWithoutVersion.xtf
    HeaderSectionWithoutSender.xtf HeaderSectionWithoutSenderAndVersion.xtf
    HeaderSectionWithoutVersion.xtf MultipleHeaderSectionDefined.xtf
    MultipleModelDefined.xtf MultipleModelsDefined.xtf
    NoModelInsideModelDefined.xtf NoModelInsideModelsDefined.xtf
    NoModelNameFound.xtf ValidHeaderSection.xtf WrongTypeInModels.xtf
)
for f in "${FILES[@]}"; do
    curl -sL "$D/$f" -o "$FIXTURE_DIR/xtf23/headerSection/$f" 2>/dev/null || true
done

# ---- Xtf23Reader/dataSection ----
D="$BASE_URL/Xtf23Reader/dataSection"
mkdir -p "$FIXTURE_DIR/xtf23/dataSection"
FILES=(
    AlignmentTypes.xtf Area.xtf AttrpathType.xtf
    BlackBoxTypes.xtf BlackBoxTypes_NoSpace.xtf BooleanType.xtf
    CommentsInFile.xtf Coord.xtf DateTimeTypes.xtf
    EmptyBasket.xtf EmptyDataSection.xtf EmptyObjects.xtf
    EnumerationTypes.xtf FormattedType.xtf MissingCoord.xtf
    MultiBaskets.xtf NumericTypes.xtf OidTypes.xtf
    OidTypesFail.xtf PolylineNoSegment.xtf PolylineWithArcs.xtf
    PolylineWithArcsNoSpace.xtf PolylineWithArcsRadius.xtf PolylineWithStraights.xtf
    References.xtf SameAttrClassTopicNames.xtf SameAttrNames.xtf
    SameClassNames.xtf SimpleCoord23a.xtf SimpleCoord23noModels.xtf
    Structures.xtf Structures2.xtf Surface.xtf
    SurfaceNoPolyline.xtf TextTypes.xtf TextTypesWithEmptyLine.xtf
    TextTypes_List.xtf TopicNameLikeClassName.xtf UndefinedSurface.xtf
    UndefinedSurface_NoSpace.xtf UnexpectedCharacters.xtf View.xtf
    WrongBoolean.xtf
)
for f in "${FILES[@]}"; do
    curl -sL "$D/$f" -o "$FIXTURE_DIR/xtf23/dataSection/$f" 2>/dev/null || true
done

# ---- Xtf23Reader/associations ----
D="$BASE_URL/Xtf23Reader/associations"
mkdir -p "$FIXTURE_DIR/xtf23/associations"
FILES=(
    CommentsInsideAssociation.xtf Embedded_0_0.xtf Embedded_0to1.xtf
    Embedded_0to1_OidAndBid.xtf Embedded_1to1_DeleteRef.xtf
    Embedded_1to1_OrderPos.xtf EmbeddedAssociationWithAttributes.xtf
    SameTargetClass.xtf SetOrderPos.xtf StandAlone.xtf
    StandAlone_WithAttributes.xtf
)
for f in "${FILES[@]}"; do
    curl -sL "$D/$f" -o "$FIXTURE_DIR/xtf23/associations/$f" 2>/dev/null || true
done

# ---- Xtf24Reader root ----
ROOT="$BASE_URL/Xtf24Reader"
FILES=(
    BasketWithConsistency.xml BasketWithDomains.xml BasketWithTransferKind.xml
    CommentsInFile.xml DeleteObject.xml DeleteObjectNoTid.xml
    EmptyBasket.xml EmptyObjects.xml EmptyTransfer.xml EmptyTransferNoSpace.xml
    MultipleBaskets.xml MultipleBasketsAndObjects.xml MultipleBasketsAndObjectsNoSpace.xml
    MultipleBasketsNoSpace.xml MultipleDataSectionDefined.xml NoDataSectionDefined.xml
    ObjectOperationMode.xml SkipBasket.xml SkipBasketFirst.xml SkipBasketOnly.xml
    StartAndEndState.xml TextBetweenLines.xml TranslationTranslatedModelName.xml
    UnexpectedCharacter.xml UnexpectedEvent.xml WrongBasketId.xml
    WrongObjectId.xml WrongTopEleName.xml WrongTopEleNamespace.xml Xml1Line.xml
)
for f in "${FILES[@]}"; do
    curl -sL "$ROOT/$f" -o "$FIXTURE_DIR/xtf24/$f" 2>/dev/null || true
done

# ---- Xtf24Reader/headerSection ----
D="$BASE_URL/Xtf24Reader/headerSection"
mkdir -p "$FIXTURE_DIR/xtf24/headerSection"
FILES=(
    CommentsBeforeModels.xml CommentsBeforeSender.xml CommentsInFile.xml
    DataBeforeHeaderSection.xml DataInsideHeaderSection.xml
    HeaderModelsAndComments.xml HeaderModelsAndSender.xml
    HeaderModelsSenderComments.xml HeaderMultipleComments.xml
    HeaderMultipleSender.xml MultipleHeaderSectionDefined.xml
    MultipleModelDefined.xml MultipleModelsDefined.xml
    NoCommentsNameFound.xml NoModelInsideModelDefined.xml
    NoModelInsideModelsDefined.xml NoModelNameFound.xml
    NoModelsDefined.xml NoSenderNameFound.xml SenderBeforeModels.xml
    WrongTypeInModel.xml WrongTypeInModels.xml Xml1Line.xml
)
for f in "${FILES[@]}"; do
    curl -sL "$D/$f" -o "$FIXTURE_DIR/xtf24/headerSection/$f" 2>/dev/null || true
done

# ---- Xtf24Reader/dataSection ----
D="$BASE_URL/Xtf24Reader/dataSection"
mkdir -p "$FIXTURE_DIR/xtf24/dataSection"
FILES=(
    AlignmentTypes.xml Area.xml AttrpathType.xml
    BlackBoxTypes.xml BooleanType.xml Coord.xml CoordNoSpace.xml
    DateTimeTypes.xml EnumerationOthers.xml EnumerationTypes.xml
    FormattedType.xml MissingCoord.xml MultiPolyline.xml
    NumericTypes.xml OidTypes.xml PolylineWithArcs.xml
    PolylineWithArcsNoSpace.xml PolylineWithArcsRadius.xml PolylineWithStraights.xml
    References.xml SameAttrClassTopicNames.xml SameAttrNames.xml
    SameClassNames.xml Structures.xml Surface.xml
    TextTypes.xml TextTypes_List.xml TopicNameLikeClassName.xml
    UndefinedSurface.xml UnexpectedCharacters.xml UnsupportedGeometry.xml
    View.xml ViewIsTransient.xml WrongBoolean.xml Xml1Line.xml
    subEnumerationOthers.xml
)
for f in "${FILES[@]}"; do
    curl -sL "$D/$f" -o "$FIXTURE_DIR/xtf24/dataSection/$f" 2>/dev/null || true
done

# ---- Xtf24Reader/associations ----
D="$BASE_URL/Xtf24Reader/associations"
mkdir -p "$FIXTURE_DIR/xtf24/associations"
FILES=(
    Alone_NtoN.xml Alone_WithAttributes.xml AssociationNotExist.xml
    CommentsInsideAssociation.xml Embedded_0_0.xml Embedded_0to1.xml
    Embedded_1to1.xml Embedded_1to1_DeleteRef.xml Embedded_1to1_OrderPos.xml
    Embedded_1toN.xml Embedded_Nto1.xml Embedded2_1to1.xml
    EmbeddedAssociationWithAttributes.xml MoreRolesThanDefined.xml
    NoAssociationName.xml RoleNotExist.xml SameTargetClass.xml
    Valid0to0Association.xml Xml1Line.xml
)
for f in "${FILES[@]}"; do
    curl -sL "$D/$f" -o "$FIXTURE_DIR/xtf24/associations/$f" 2>/dev/null || true
done

# ---- Xtf24Writer/dataSection ----
D="$BASE_URL/Xtf24Writer/dataSection"
mkdir -p "$FIXTURE_DIR/xtf24writer/dataSection"
FILES=( MultiCoord.xtf MultiPolyline.xtf MultiSurfaceArea.xtf PolylineWithStraights.xtf References.xtf )
for f in "${FILES[@]}"; do
    curl -sL "$D/$f" -o "$FIXTURE_DIR/xtf24writer/dataSection/$f" 2>/dev/null || true
done

# Clean up 404s (files that are just "404: Not Found")
echo "=== Cleaning 404 errors ==="
find "$FIXTURE_DIR" -type f -size -20c -exec rm {} \; -exec echo "  removed empty: {}" \;

# Count
total=$(find "$FIXTURE_DIR" -type f | wc -l)
echo "=== Done: $total fixture files ==="
