#include "iox/Events.h"
#include "iox/test/Test.h"

int main() {
    iox::IoxEvent event = iox::EndTransferEvent{};
    IOX_CHECK(iox::eventKind(event) == iox::EventKind::EndTransfer);
    IOX_CHECK_EQ(std::string("endTransfer"),
                 std::string(iox::eventKindName(iox::eventKind(event))));
    IOX_CHECK_EQ(std::string("Object"),
                 std::string(iox::eventTypeName(iox::IoxEvent{iox::ObjectEvent{}})));
    return 0;
}
