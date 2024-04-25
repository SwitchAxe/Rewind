#pragma once
#include "branch.hpp"
#include "io.hpp"
#include "string.hpp"
#include "numeric.hpp"
#include "list.hpp"
#include "boolean.hpp"
#include "misc.hpp"
#include "source.hpp"

static std::map<std::string, Functor> procedures =
  combine(branching,
	  combine(io,
		  combine(string,
			  combine(numeric,
				  combine(list,
					  combine(code,
						  misc))))));
