#include "iox/Version.h"

#ifdef IOX_CONSUMER_HAS_ILIC
#include "iox/ilic/IlicModelIndex.h"
#endif

#include <string>

int main()
{
    if (std::string(iox::version()).empty()) {
        return 1;
    }

#ifdef IOX_CONSUMER_HAS_ILIC
    metamodel::MetaModelStore store;
    iox::ilic::IlicModelIndex index(store);
    (void)index;
#endif

    return 0;
}
