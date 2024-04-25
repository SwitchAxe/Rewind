#pragma once
#include "types.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "procedures.hpp"
Symbol eval(Symbol root, const path& PATH, variables& vars, int line);
