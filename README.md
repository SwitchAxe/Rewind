# Rewind
A Scheme-inspired shell and a programming language.

# Building
Build with `make` and run with `./rewind`.

# Dependencies
* Matchit (for pattern matching in some code regions).
  - You can find it [here](https://github.com/BowenFu/matchit.cpp)

# Examples
Once started, Rewind will greet you with a very barebone REPL in which you may enter any
valid Rewind expression. The special expressions `exit` and `(exit)` terminate the program.
Below is an up-to-date list of examples for all the (not many!) things that Rewind can currently do.
- `(+ 1 2 3)`
  * This will evaluate the sum of 1, 2, and 3, yielding 6.
- `(* 3 (let x 3))`
  * This first defines x with the value 3, and then multiplies it by 3, giving the
  user 9.
  Note, you can also repeat the newly bound variable x in the _same_ expression it was defined in,
  and in any inner subexpressions, but not in any level above, which in this case means the level of
  the * operator. Thus, this is valid:
  `(* 3 (let x 3) x) => 27` (the arrow is just to express the return value, and is not part of the
  expression).
- The same `let` keyword can also be used for function definitions, like so:
  * `(+ 1 (let id (x) x) (id 5)) => 6`.
  Please do note that a function definition does **not** return any processable value to the caller.
  Any amount of arguments can appear in a definition; This is permitted:
  `(+ 1 (let sum (a b) (+ a b)) (sum 4 5)) => 10`.
  Variadic functions, template functions, or lambda functions are not presently supported.
  * You can also do simple recursion, and it should work:
	* `(+ (let ! (n) (if n (* n (! (- n 1))) 1)) (! 5)) => 120`
- Rewind has rudimentary support for external programs and pipes:
  * `(ls -al)` runs `ls -al` on the current directory;
  * `(-> (ls -a) (grep make) (cat) (tr -d '\n') (xargs wc))` is an overcomplicated way to run `wc` on the
	makefile of the project, assuming you're situated in the root directory.
