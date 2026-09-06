#include <iostream>
#include "storage/StorageEngine.h"

int main() {
    StorageEngine storage;

    storage.put(1, "Akhil");
    storage.put(2, "AdaptiveCore");

    auto value = storage.get(1);

    if (value.has_value()) {
        std::cout << "GET 1: " << *value << std::endl;
    }

    bool removed = storage.remove(1);

    if (removed) {
        std::cout << "DELETE 1: Success" << std::endl;
    }

    value = storage.get(1);

    if (!value.has_value()) {
        std::cout << "GET 1: NOT FOUND" << std::endl;
    }

    return 0;
}