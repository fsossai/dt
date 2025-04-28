#pragma once

#include "BaseSequence.hpp"

namespace skynet {

template <typename T>

using Sequence = BaseSequence<T, /*Order=*/true>;

}
