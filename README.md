# Goon

A 1k line embeddable configuration language with Nix-like syntax.

## Example

```goon
let name = "goon";
let version = 1;

let colors = import("./colors");

{
    name = name;
    version = version;

    border = {
        width = 2;
        color = colors.blue;
    };

    tags = ["1", "2", "3", "4", "5"];
}
```

## CLI Options

```bash
goon eval config.goon
goon check config.goon
```

## Embedding

Copy `src/goon.c` and `src/goon.h` into your project.

```c
#include "goon.h"

int main() {
    Goon_Ctx *ctx = goon_create();

    if (goon_load_file(ctx, "config.goon")) {
        Goon_Value *result = goon_eval_result(ctx);
        Goon_Value *name = goon_record_get(result, "name");
        if (goon_is_string(name)) {
            printf("name: %s\n", goon_to_string(name));
        }
    } else {
        printf("error: %s\n", goon_get_error(ctx));
    }

    goon_destroy(ctx);
    return 0;
}
```
