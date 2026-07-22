#pragma once

#include <memory>

#include "im/DirectMessagePersistenceTypes.h"

namespace storage {
class DirectMessageWriteStore;
}

namespace im {

class DirectMessagePersistenceService {
public:
    explicit DirectMessagePersistenceService(std::shared_ptr<storage::DirectMessageWriteStore> writeStore);

    DirectMessageWriteResult persist(const DirectMessageWriteCommand& command) const;

private:
    std::shared_ptr<storage::DirectMessageWriteStore> writeStore_;
};

}