# Ownership regressions

These ASan/UBSan tests cover 4D native ownership in the plugin's commands and in `support/4DPlugin-JSON.cpp`. They compile the real plugin sources (`4DPlugin-Ultralight.cpp`, `IFileSystem.cpp`, `support/4DPlugin-JSON.cpp`) and the full, unchanged bundled `4D Plugin API`. They use a synthetic `Call4D` engine that tracks every engine string, picture, handle and object.

Nothing is rendered or fetched. `ultralight_stubs.cpp` replaces the Ultralight renderer, views, bitmaps, `JSHelpers` and libpng with synthetic implementations:
- Views only record what they were asked to load.
- Script evaluation echoes the script.
- The PNG encoder emits only the signature.
- `String16` buffers are length-counted, with no trailing NUL, as in the real library.

```sh
# fixed tree: every test must pass
tests/ownership/run.sh ultralight fixed

# immutable baseline: defect/hardening tests must fail, controls must pass
mkdir /tmp/base && git archive <base-sha> | tar -x -C /tmp/base
tests/ownership/run.sh /tmp/base/ultralight baseline
```

Only macOS is covered. The Windows build, the real 4D runtime and the real Ultralight/WebCore engine are not exercised.
