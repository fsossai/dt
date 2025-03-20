#pragma once

#include <vector>
#include <iostream>
#include <cassert>

// #define DEBUG

#include "Array.hpp"
#include "BaseSequence.hpp"
#include "EnumSet.hpp"
#include "HSequence.hpp"
#include "IV.hpp"
#include "MArray.hpp"
#include "MEnumSet.hpp"
#include "MSequence.hpp"
#include "PSequence.hpp"
#include "Range.hpp"
#include "Scalar.hpp"
#include "Set.hpp"

namespace skynet {

template <typename T>
using Sequence = BaseSequence<T, /*Order=*/true>;

template <typename T>
using Bag = BaseSequence<T, /*Order=*/false>;

} // namespace skynet
