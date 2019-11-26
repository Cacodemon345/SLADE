<article-head>SLADE Scripting Documentation</article-head>

Here is the documentation for the SLADE Lua scripting system.

## Notes

### Running Scripts

State is not presereved after running a script - eg. a global variable defined in a script will not be available next time a script is run

### Type Properties

Type properties can either be read-only or read+write. Trying to modify a property that is read only will result in a script error.

In this documentation, a symbol is displayed to the left of each property indicating whether the property can be modified:

* <prop class="ro">property</prop>: Read-only, can not be modified
* <prop class="rw">property</prop>: Can be modified

## Lua Language

Since the SLADE scripting functionality uses Lua as its language, it is recommended to read up on Lua ([Programming in Lua](https://www.lua.org/pil/contents.html)).

Below are some general notes about some things that may trip you up if you're used to other languages such as C or ZScript.

### Array and List Indices

Lua differs from most other languages in the way array indices work - usually, the first element of an array is index `0`, but in Lua the first element will be index `1`.

SLADE scripting functions & types use the Lua (`1`-based) convention by default, except where it makes more sense to begin at `0`. As an example, map objects (lines, sectors, etc.) start at index `0` in the game and editor, so these will also begin at `0` in scripts.

This documentation will also specifically mention if an index is `0`-based, otherwise it can be assumed to start at `1`.

### Type Member Functions

Any function that is a member of a type has `self` as the first parameter, which is how 'member functions' work in lua.

Lua provides the `:` shortcut to call member functions with an automatic `self` parameter:

```lua
local object = SomeType.new()

-- The following lines are equivalent
object:SomeFunction(param1, param2)
object.SomeFunction(object, param1, param2)
SomeType.SomeFunction(object, param1, param2)
```

It is generally recommended to use the `:` format in these cases, as it is shorter and less error-prone.
