# Goon

A tiny embeddable configuration language with Nix-like syntax.

- **~1800 lines of C** - zero dependencies
- **39KB binary** - smaller than most config parsers
- **JSON output** - integrates with anything
- **Arrow functions** - define your own config templates

## Quick Example

```goon
let key = (mods, key, cmd) => { mods = mods; key = key; cmd = cmd; };

let keys = map([1..9], (n) => key("super", n, "workspace ${n}"));

{
    border_width = 2;
    gap = 10;
    keys = keys;
}
```

Output:
```json
{
  "border_width": 2,
  "gap": 10,
  "keys": [
    { "mods": "super", "key": 1, "cmd": "workspace 1" },
    { "mods": "super", "key": 2, "cmd": "workspace 2" },
    ...
  ]
}
```

## Features

- **Let bindings**: `let x = 1;`
- **Records**: `{ name = "foo"; value = 42; }`
- **Lists**: `[1, 2, 3]`
- **Ranges**: `[1..9]` expands to `[1, 2, 3, 4, 5, 6, 7, 8, 9]`
- **Arrow functions**: `(x) => { doubled = x; }`
- **String interpolation**: `"value is ${x}"`
- **Spread operator**: `{ ...defaults; custom = 1; }`
- **Imports**: `import("./other.goon")`
- **Conditionals**: `if cond then a else b` or `cond ? a : b`

Why C? Because C has the best C interop.

Requires only a C99 compiler.

## Embedding

Copy `src/goon.c` and `src/goon.h` into your project.

```c
#include "goon.h"

int main() {
    Goon_Ctx *ctx = goon_create();

    if (!goon_load_file(ctx, "config.goon")) {
        goon_error_print(goon_get_error_info(ctx));
        goon_destroy(ctx);
        return 1;
    }

    Goon_Value *result = goon_eval_result(ctx);

    // Access fields
    Goon_Value *name = goon_record_get(result, "name");
    if (goon_is_string(name)) {
        printf("name: %s\n", goon_to_string(name));
    }
// Or convert to JSON
    char *json = goon_to_json_pretty(result, 2);
    printf("%s\n", json);
    free(json);

    goon_destroy(ctx);
    return 0;
}
```

### Registering Custom Functions

```c
Goon_Value *my_func(Goon_Ctx *ctx, Goon_Value **args, size_t argc) {
    if (argc < 1 || !goon_is_int(args[0])) return goon_nil(ctx);
    return goon_int(ctx, goon_to_int(args[0]) * 2);
}

Goon_Ctx *ctx = goon_create();
goon_register(ctx, "double", my_func);
```

## Why Goon?

| Language | Deps | Size | Turing Complete |
|----------|------|------|-----------------|
| **Goon** | 0 | 39KB | No |
| Lua | 0 | ~250KB | Yes |
| Nickel | Many | ~20MB | Yes |
| Dhall | Many | ~50MB | No |

Use goon for configs.

## Documentation

- [SPEC.md](SPEC.md) - Language specification
- [examples/](examples/) - Example configs
