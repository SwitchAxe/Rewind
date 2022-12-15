# Rewind
A Scheme-inspired shell and a programming language.

# Building
Build with `make` and run with `./rewind`.

# Examples
Once started, Rewind will greet you with a very barebone REPL which will
take precisely one expression, evaluate it to the (hopefully) correct result,
and exit. Here are some of the things Rewind can currently do (not much, admittedly):
- `(+ 1 2 3)`
  * This will evaluate the sum of 1, 2, and 3, yielding 6.
- `(* 3 (let x 3))`
  * This first defines x with the value 3, and then multiplies it by two, giving the
  user 9.
  Note, you can also repeat the newly bound variable x in the _same_ expression it was defined in,
  and in any inner subexpressions, but not in any level above, which in this case means the level of
  the * operator. Thus, this is valid:
  `(* 3 (let x 3) x) => 9` (the arrow is just to express the return value, and is not part of the
  expression).
- The same `let` keyword can also be used for function definitions, like so:
  * `(+ 1 (let id (x) x) (id 5)) => 6`.
  Please do note that a function definition does **not** return any processable value to the caller.
  Any amount of arguments can appear in a definition; This is permitted:
  `(+ 1 (let sum (a b) (+ a b)) (sum 4 5)) => 9`.
  Variadic functions, template functions, or lambda functions are not presently supported.
