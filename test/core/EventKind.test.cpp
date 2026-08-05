#include "iox/Events.h"
#include "iox/test/Test.h"

int main() {
    IOX_CHECK(iox::eventKind(iox::IoxEvent{iox::StartTransferEvent{}}) ==
              iox::EventKind::StartTransfer);
    IOX_CHECK(iox::eventKind(iox::IoxEvent{iox::StartBasketEvent{}}) ==
              iox::EventKind::StartBasket);
    IOX_CHECK(iox::eventKind(iox::IoxEvent{iox::ObjectEvent{}}) ==
              iox::EventKind::Object);
    IOX_CHECK(iox::eventKind(iox::IoxEvent{iox::EndBasketEvent{}}) ==
              iox::EventKind::EndBasket);
    iox::IoxEvent event = iox::EndTransferEvent{};
    IOX_CHECK(iox::eventKind(event) == iox::EventKind::EndTransfer);
    IOX_CHECK_EQ(std::string("endTransfer"),
                 std::string(iox::eventKindName(iox::eventKind(event))));
    return 0;
}
