#ifndef FUNCTION_IMPL_HPP
#define FUNCTION_IMPL_HPP

#include <etl/delegate.h>

#define XBOT_FUNCTION_TYPEDEF etl::delegate
#define XBOT_FUNCTION_FOR_METHOD(class, method, instance) etl::make_delegate<class, method>(*instance)

#endif  // FUNCTION_IMPL_HPP
