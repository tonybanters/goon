# Goon Language Specification

Version: 0.1.0

Goon is an embeddable configuration language with Nix-like syntax. It evaluates to JSON and is designed for window manager configs and similar applications.

## Lexical Structure

### Comments

```goon
// Single line comment

/* Multi-line
   comment */
```

### Identifiers

```
IDENT = [a-zA-Z_][a-zA-Z0-9_]*
```

Reserved keywords: `let`, `if`, `then`, `else`, `true`, `false`, `import`

### Integers

```
INT = -?[0-9]+
```

Examples: `0`, `42`, `-10`

### Strings

Double-quoted with escape sequences and interpolation:

```goon
"hello world"
"line1\nline2"
"value is ${x}"
```

Escape sequences:
- `\n` - newline
- `\t` - tab
- `\r` - carriage return
- `\\` - backslash
- `\"` - double quote
- `\$` - literal dollar sign (escape interpolation)

Interpolation:
- `${identifier}` is replaced with the string value of the variable
- Integers and booleans are converted to strings automatically

### Booleans

```goon
true
false
```

### Operators and Punctuation

| Token | Description |
|-------|-------------|
| `=`   | Assignment |
| `=>`  | Arrow (lambda) |
| `..`  | Range |
| `...` | Spread |
| `?`   | Ternary condition |
| `:`   | Ternary separator |
| `;`   | Statement terminator |
| `,`   | List/argument separator |
| `.`   | Field access |
| `{}`  | Record delimiters |
| `[]`  | List delimiters |
| `()`  | Grouping / parameters |

## Grammar

```ebnf
program     = statement* expression? ;

statement   = let_binding ;
let_binding = "let" IDENT "=" expression ";" ;

expression  = if_expr
            | ternary ;

if_expr     = "if" expression "then" expression "else" expression ;

ternary     = primary ("?" expression ":" expression)? ;

primary     = INT
            | STRING
            | "true" | "false"
            | IDENT ("(" args? ")")?      (* variable or function call *)
            | IDENT ("." IDENT)*          (* field access *)
            | record
            | list
            | lambda
            | import_expr
            | "(" expression ")" ;

lambda      = "(" params? ")" "=>" expression ;
params      = IDENT ("," IDENT)* ;
args        = expression ("," expression)* ;

record      = "{" (record_item (";" record_item)* ";"?)? "}" ;
record_item = IDENT "=" expression
            | "..." expression ;

list        = "[" (list_item ("," list_item)* ","?)? "]" ;
list_item   = range
            | "..." expression
            | expression ;

range       = INT ".." INT ;

import_expr = "import" "(" STRING ")" ;
```

## Types

Goon has the following value types:

| Type | Description | JSON Output |
|------|-------------|-------------|
| `nil` | Null value | `null` |
| `bool` | Boolean | `true` / `false` |
| `int` | 64-bit integer | Number |
| `string` | UTF-8 string | String |
| `list` | Ordered collection | Array |
| `record` | Key-value map | Object |
| `lambda` | Function | (not serializable) |

## Semantics

### Let Bindings

Bind a name to a value. Semicolon is required.

```goon
let x = 42;
let name = "goon";
```

Bindings are visible after their definition in the same scope.

### Records

Records are key-value maps with string keys.

```goon
{
    name = "goon";
    version = 1;
    nested = {
        foo = "bar";
    };
}
```

Field access with dot notation:

```goon
let config = { name = "test"; };
let n = config.name;  // "test"
```

### Lists

Ordered collections of values.

```goon
[1, 2, 3]
["a", "b", "c"]
[{ x = 1; }, { x = 2; }]
```

### Ranges

Ranges expand to inclusive integer sequences inside lists.

```goon
[1..5]      // [1, 2, 3, 4, 5]
[1..1]      // [1]
```

### Spread Operator

Spread (`...`) merges values into lists or records.

In lists:
```goon
let a = [1, 2];
let b = [...a, 3, 4];  // [1, 2, 3, 4]
```

In records:
```goon
let defaults = { gap = 10; border = 2; };
let config = { ...defaults; gap = 20; };  // { gap = 20; border = 2; }
```

Later values override earlier ones.

### Arrow Functions (Lambdas)

Anonymous functions for creating templates.

```goon
let double = (x) => { value = x; doubled = x; };
let make_key = (mod, key, cmd) => { mod = mod; key = key; cmd = cmd; };
```

Functions are called with parentheses:

```goon
let result = double(5);           // { value = 5; doubled = 5; }
let k = make_key("super", "a", "app");
```

Functions capture their lexical environment (closures).

### String Interpolation

Variables can be embedded in strings:

```goon
let name = "world";
let greeting = "hello ${name}";  // "hello world"

let n = 42;
let msg = "value is ${n}";       // "value is 42"
```

### Conditionals

If-then-else:
```goon
let x = if true then 1 else 2;   // 1
```

Ternary operator:
```goon
let x = true ? 1 : 2;            // 1
```

Both require an else branch.

### Imports

Import other goon files:

```goon
let colors = import("./colors.goon");
let theme = colors.dark;
```

- Paths are relative to the importing file
- `.goon` extension is optional
- Imported files are evaluated and their final expression is returned

## Built-in Functions

### map(list, function)

Apply a function to each element of a list.

```goon
let nums = [1..3];
let doubled = map(nums, (n) => n);  // [1, 2, 3] (identity)

let keys = map([1..9], (n) => {
    mod = "super";
    key = n;
    cmd = "workspace ${n}";
});
```

## Constraints

1. **No arithmetic**: Goon does not have `+`, `-`, `*`, `/` operators
2. **No comparison**: No `==`, `<`, `>` operators
3. **No recursion**: Functions cannot call themselves
4. **Immutable**: Values cannot be reassigned after binding

## Output

Goon evaluates to a single value, typically a record, which is serialized to JSON.

```goon
let name = "myapp";
let version = 1;

{
    name = name;
    version = version;
    enabled = true;
}
```

Output:
```json
{
  "name": "myapp",
  "version": 1,
  "enabled": true
}
```

## CLI Usage

```bash
# Evaluate and output JSON
goon eval config.goon

# Pretty-print output
goon eval config.goon --pretty

# Check syntax without evaluating
goon check config.goon

# Show version
goon --version
```

## C API

```c
#include "goon.h"

// Create context
Goon_Ctx *ctx = goon_create();

// Load and evaluate
if (!goon_load_file(ctx, "config.goon")) {
    const Goon_Error *err = goon_get_error_info(ctx);
    goon_error_print(err);
    return 1;
}

// Get result
Goon_Value *result = goon_eval_result(ctx);

// Convert to JSON
char *json = goon_to_json_pretty(result, 2);
printf("%s\n", json);
free(json);

// Cleanup
goon_destroy(ctx);
```

### Registering Built-in Functions

```c
Goon_Value *my_builtin(Goon_Ctx *ctx, Goon_Value **args, size_t argc) {
    // Implementation
    return goon_int(ctx, 42);
}

goon_register(ctx, "my_func", my_builtin);
```

## Future Considerations

The following features may be added in future versions:

- Arithmetic operators (`+`, `-`, `*`, `/`, `%`)
- Comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`)
- Logical operators (`&&`, `||`, `!`)
- Optional type annotations
- Hex integer literals (`0xFF`)
- Filter in map: `map(list, fn, predicate)`
