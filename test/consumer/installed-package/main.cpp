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
    metamodel::Model model;
    model.Name = "InstalledPackageSmoke";
    iox::ilic::IlicModelIndex index(model);
    if (index.findTopic("Missing.Topic") != nullptr) {
        return 2;
    }
#endif

    return 0;
}
