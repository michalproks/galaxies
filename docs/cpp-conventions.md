# C++ Coding Conventions

**Files** — `Foo.cpp` and `Foo.h` side by side; tests in the same directory as `Foo.Test.cpp` (plus `Foo.Test.h` if needed).

**Code** — Default to `const`: methods that do not mutate state, and variables not reassigned. Always use `override` when overriding `virtual`.

**Comments** — Document all interfaces: side effects, input constraints, invariants. Skip implementation commentary unless non-obvious.

**Naming** — Types (structs, classes, enums): `PascalCase`. Methods, free functions, arguments, locals, public members: `camelCase`. Compile-time constants (incl. enum values): `ALL_CAPS`. Non-public static members: `sPascalCase`. Non-public non-static members: `mPascalCase`. Globals: `gPascalCase`.
