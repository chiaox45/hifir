# HIFIR Unit Testing #

To add a new test, create a file with `test_*.cpp`. For instance, let's make a new unit testing `test_foo.cpp`.

```cpp
#include "common.hpp" // must include this before any other hif*.hpp

#include "hif/ds/Array.hpp"
#include "hif/utils/log.hpp"

#include <gtest/gtest.h>

TEST(...) {...}
```

CMake auto-discovers all `test_*.cpp` files — no manual target registration needed. To build and run a single test:

```console
cmake --build --preset debug
ctest --preset debug -R test_foo
```

## Hint ##

It's recommended that you run tests with AddressSanitizer to check for memory leaks:

```console
cmake --preset asan && cmake --build --preset asan && ctest --preset asan
```
