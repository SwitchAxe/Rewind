# Rewind
A Scheme-inspired shell and a programming language.

# Building
Build with `make` and run with `./rewind`.  
Optionally, you can install the program (after a successful build) with  
`[sudo] make install`. `sudo` is optional. If `sudo` is present, Rewind will be installed in  
`/usr/bin/rewind`. Otherwise, it will be installed in `$HOME/.local/bin/rewind`.
# Dependencies
* Matchit (for pattern matching in some code regions).
  - You can find it [here](https://github.com/BowenFu/matchit.cpp)
* GNU Readline.

# Examples
Once started, Rewind will greet you with a very barebone REPL in which you may enter any
valid Rewind expression. The special expressions `exit` and `(exit)` terminate the program.  
For a detailed introduction to the language, and a reference for all its operators and their semantics, see the 
Rewind Github wiki.

# Custom prompts and the Rewind config file

Rewind supports a config file for easy storing of common variables and functions you might want to preserve in 
between interactive sessions. Optionally, if the very last expression in the config file is a function call that
returns a string, or it's a string literal, that will be chosen as the interactive prompt for Rewind. The following is
an example of a valid prompt snippet: (in your ~/.config/config.re)

```
let prompt ()
  s+ (get PWD) " λ ";

prompt
```
Unsurprisingly, the resulting Rewind prompt will be like this (if you're in the root directory of Rewind):

`/home/<user>/path/to/Rewind λ `
with your username instead of `<user>`, and the full path to Rewind before the lambda, with no shortenings whatsoever.

