# Expression VM (`CF_ExpressionVM`)

A small register-stack VM for evaluating arithmetic expressions at runtime.
Designed to compile once and evaluate cheaply many times. All values are
floats. Lives in `3_Game/.../ExpressionVM/`.

## Two surface syntaxes

| Type                | Spawn typename     | Looks like                       |
|---------------------|--------------------|----------------------------------|
| `CF_MathExpression` | default            | `4 * factor(speed, 0, 100)`       |
| `CF_SQFExpression`  | DayZ SoundShader-style | `4 * (speed factor [0, 100])` |

Both compile to the same instruction pipeline.

## API (`CF_ExpressionVM`)

```csharp
auto e = CF_ExpressionVM.Compile("5 * 5");                              // default = CF_MathExpression
auto e = CF_ExpressionVM.Compile("4 * factor(speed,0,100)", {"speed"}); // with variables
auto e = CF_ExpressionVM.Compile("4 * (speed factor [0,100])",
                                 {"speed"}, CF_SQFExpression);

float v = e.Evaluate();                          // no variables
float v = e.Evaluate({50.0});                    // values for declared variables, in order
```

Order matters: `e.Evaluate(values)` requires `values.Count() ==
declaredVariables.Count()`. The VM does not check.

`CF_ExpressionVM.Find(name, out def)` — looks up a registered function.

`CF_ExpressionVM.AddFunction(name, def)` — registers a new function. Called
internally for built-ins via `[CF_EventSubscriber(..., OnGameCreate)]`.

`CF_ExpressionVM.Destroy()` — wipes the function map on `OnGameDestroy`.

## Built-in functions

(From `CF_ExpressionFunction.c`; all registered on `OnGameCreate`.)

| Name             | Token   | Params | Precedence | Associative | Notes |
|------------------|---------|-------:|-----------:|:-----------:|-------|
| `CF_ExpressionFunctionValue`    | `#INTERNAL_0` |   -1 | – | – | Pushes a literal float. |
| `CF_ExpressionFunctionVariable` | `#INTERNAL_1` |   -1 | – | – | Pushes a runtime variable. |
| `CF_ExpressionFunctionPow`      | `^`     | 0 | 4 | false | `a^b` (in-stack). |
| `CF_ExpressionFunctionMul`      | `*`     | 0 | 3 | true  | |
| `CF_ExpressionFunctionDiv`      | `/`     | 0 | 3 | true  | |
| `CF_ExpressionFunctionAdd`      | `+`     | 0 | 2 | true  | |
| `CF_ExpressionFunctionSub`      | `-`     | 0 | 2 | true  | |
| `CF_ExpressionFunctionFactor`   | `factor`| 2 | 1 | true  | `(clamp(x,a,b)-a)/(b-a)`. Compile-time optimization swaps to `#factor_reverse` if `a>b`. |
| `CF_ExpressionFunctionReverseFactor` | `#factor_reverse` | 2 | 1 | true | Internal optimization target. |
| `CF_ExpressionFunctionCos`      | `cos`   | 0 | 1 | true  | `Math.Cos(top)`. |
| `CF_ExpressionFunctionSin`      | `sin`   | 0 | 1 | true  | `Math.Sin(top)`. |
| `CF_ExpressionFunctionMin`      | `min`   | 0 | 1 | true  | Top two stack entries. |
| `CF_ExpressionFunctionMax`      | `max`   | 0 | 1 | true  | Top two stack entries. |

`params` here is the count of **compile-time** parameters (those evaluated at
compile time and stored as `param1..param4` on the instruction). The first
"runtime parameter" is whatever's already on the stack — that's why `Pow`,
`Mul`, etc. report `params=0`.

## Custom function (from docs)

```csharp
class ExpressionFunctionPow : CF_ExpressionFunction
{
    static string CF_NAME = "pow";

    [CF_EventSubscriber(ExpressionFunctionPow.Init, CF_LifecycleEvents.OnGameCreate)]
    static void Init()
    {
        CF_ExpressionVM.AddFunction(CF_NAME,
            new CF_ExpressionFunctionDef(ExpressionFunctionPow,
                /*params*/1, /*precedence*/1, /*associative*/true));
    }

    override void Call()
    {
        CF_ExpressionVM.Stack[CF_ExpressionVM.StackPointer] =
            Math.Pow(CF_ExpressionVM.Stack[CF_ExpressionVM.StackPointer], param1);
    }

    override string ToStr() { return CF_NAME; }
}
```
Note: the docs show passing the typename directly to `AddFunction(name, type)`,
but the actual signature in `CF_ExpressionVM.c` is
`AddFunction(string name, notnull CF_ExpressionFunctionDef function)` — so
wrap the typename in a new `CF_ExpressionFunctionDef`.

## Instruction representation

`CF_ExpressionInstruction`:
- `string token`, `float value`, `int variableIndex`
- `ref CF_ExpressionInstruction next` — singly-linked list
- `float param1..param4` — flattened (not an array, for speed)
- `void Call()` — virtual; subclasses override

`CF_ExpressionFunction extends CF_ExpressionInstruction`. New function classes
override `Call()` and `ToStr()`.

## Evaluator

`CF_Expression._Evaluate(variables)`:
1. `CF_ExpressionVM.StackPointer = 0`, `Variables = variables`.
2. Walk the instruction list, calling `instruction.Call()` on each.
3. Return `CF_ExpressionVM.Stack[StackPointer]`.

Stack: `static float Stack[16]` — fixed depth, no overflow check. Don't write
expressions that nest more than 16 deep without testing first.

## Compiler

`CF_MathExpression.__Compile` — Shunting-yard algorithm to convert tokenized
input into the instruction list. `CF_SQFExpression._Compile` is similar but
parameter syntax is `function [a,b,...]` (square brackets).

A neat optimization: `factor` whose `(min, max)` second arg has `max < min` is
compiled to `#factor_reverse` so the `Math.Clamp` arguments are already in
correct order at runtime.

## Reverse-Polish debug

`expr.ToRPN()` returns a debug string of the compiled instructions. Useful for
inspecting that compilation produced what you expected.

## Tests

`CF_ExpressionTests.c` (lots of tests inside the source, ~580 lines) — gated
behind `CF_EXPRESSION_TEST` define. The companion file
`ExpressionVM/DayZGame.c` runs `CF_ExpressionTests.PerformSingle("TestPerformance")`
three times during play (every 5 seconds for the first 15 seconds) when that
define is enabled.
