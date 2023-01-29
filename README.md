# Rewind
A Scheme-inspired shell and a programming language.

# Building
Build with `make` and run with `./rewind`.

# Dependencies
* Matchit (for pattern matching in some code regions).
  - You can find it [here](https://github.com/BowenFu/matchit.cpp)
* GNU Readline.

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
  On the other hand, Rewind also supports Higher Order Functions in the sense that you can pass functions
  as arguments to other functions:
  ```
    (+ (let o (f g x) (f (g x)))
       (let sqr (x) (* x x))
       (let +1 (x) (+ x 1))
       (o sqr +1 5)) => 36
  ```
  * You can also do simple recursion, and it should work:
	* `(+ (let ! (n) (if n (* n (! (- n 1))) 1)) (! 5)) => 120`
- Rewind has rudimentary support for external programs and pipes:
  * `(ls -al)` runs `ls -al` on the current directory;
    - Assigning output to variables instead of writing to stdout is also possible, in which case the
      variable will hold a string representation of the command output:
      * `(+ 1 (toi (let x (echo "3")))) => 4`.
  * `(-> (ls -a) (grep make) (cat) (tr -d '\n') (xargs wc))` is an overcomplicated way to run `wc` on the
	makefile of the project, assuming you're situated in the root directory.
- There's also the ability to set, and get, environment variables for the current Rewind session:
  * `(get PATH) => your $PATH`;
  * `(set PATH src) => sets $PATH to 'src'` (assuming you're in the root directory of the project).
- You can write and append to files with the `+>` and `++>` operators, respectively:
  * `(+> "This will create a file if test.txt does not exist" test.txt)`;
  * `(++> "this text will be appended at the end of the file" test.txt)`;
- Rewind has support for redirecting program output to files, also:
  * `(> (-> (ls -al)) log.txt)` will _append_ the output of `ls -al` to `log.txt`.
    Note: the file is created if it doesn't exist.
- There's the possibility, if need be, to pass specific environment variables to programs. The syntax is
  as follows:  
  * `(-> ((ENV1 something) ls -al) ((ENV2 whatever) (IDK idk) grep src -))`;
  * `((A a) (B b) cat makefile)`.
  currently booleans, strings, and numbers will be parsed correctly. Each will be translated into a string
  representation of the value: `true` => "true", 42 => "42".
  The program _should_ run fine if instead of barewords (strings without leading and trailing '"') you put string
  literals, but i haven't tested that scenario.
- The _reference_ (`$`) operator allows you to explicitly reference variables inside external commands:
  * `(+ (let x 4) (+ 1 (toi (let y (echo ($ x)))))) => 9`.  
    - While it _can_ be used for referencing variables even outside of external commands, it is not strictly
      necessary in that scenario.

# Custom prompts and the Rewind config file

Rewind supports a config file for easy storing of common variables and functions you might want to preserve in 
between interactive sessions. Optionally, if the very last expression in the config file is a function call that
returns a string, or it's a string literal, that will be chosen as the interactive prompt for Rewind. The following is
an example of a valid prompt snippet: (in your ~/.config/config.re)

```
(let prompt ()
        (nostr (s+ (get PWD)
                           (nostr " λ")
                           (nostr " "))))

(prompt)
```
Unsurprisingly, the resulting Rewind prompt will be like this (if you're in the root directory of Rewind):

`/home/<user>/path/to/Rewind λ `
with your username instead of <user>, and the full path to Rewind before the lambda, with no shortenings whatsoever.

