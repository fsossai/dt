#pragma once

#include "BaseSequence.hpp"

namespace skynet {

template <typename T>
using Bag = BaseSequence<T, /*Order=*/false>;

}
