// Compile-only consumer smoke test. Keeping this as a separate executable
// catches accidental dependencies on private include paths or build order.
#include "iox/Basket.h"
#include "iox/ByteView.h"
#include "iox/Diagnostic.h"
#include "iox/Events.h"
#include "iox/Factory.h"
#include "iox/FormatRegistry.h"
#include "iox/IomName.h"
#include "iox/IomObject.h"
#include "iox/IomValue.h"
#include "iox/Reader.h"
#include "iox/Version.h"
#include "iox/Writer.h"
#include "iox/abi/iox.h"
#include "iox/json/JsonEventReader.h"
#include "iox/json/JsonEventWriter.h"
#include "iox/xtf/Xtf23Dialect.h"
#include "iox/xtf/Xtf24Dialect.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfReaderOptions.h"
#include "iox/xtf/XtfVersion.h"
#include "iox/xtf/XtfWriter.h"

int main() {
    return iox_abi_version() > 0 ? 0 : 1;
}
