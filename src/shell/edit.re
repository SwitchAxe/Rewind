let re_color_black = "\e[0;30m";
let re_color_red = "\e[0;31m";
let re_color_green = "\e[0;32m";
let re_color_orange = "\e[0;33m";
let re_color_blue = "\e[0;34m";
let re_color_purple = "\e[0;35m";
let re_color_cyan = "\e[0;36m";
let re_color_lgray = "\e[0;37m";
let re_color_dgray = "\e[1;30m";
let re_color_lred = "\e[1;31m";
let re_color_lgreen = "\e[1;32m";
let re_color_yellow = "\e[1;33m";
let re_color_lblue = "\e[1;34m";
let re_color_lpurple = "\e[1;35m";
let re_color_lcyan = "\e[1;36m";
let re_color_white = "\e[1;37m";
let re_color_blank = "\e[0m";
let re_color_attr_bold = "\e[1m";

# cursor movement forward/backwards

let curs_fwd = (n) => cond
    	       	      | (= n 0) => "",
		      | _ => (s+ "\e[" (tos n) "C");
let curs_bwd = (n) => cond
    	       	      | (= n 0) => "",
		      | _ => (s+ "\e[" (tos n) "D"); 

# erase the current line, starting from the cursor
let curs_erase = "\e[0K";

let getch = () => {
    let x = (readch)
    cond
    | (!= (chtoi $x) 27) => $x,
    | (!= (chtoi (readch)) 91) => "undefined",
    | _ => (readch);
}

let strlen = (s) => (length (stol s));
let overwrite = (w c) => { print (curs_bwd (strlen w)) c w $re_color_blank; w; }
let kind = (x) => (typeof (ast (tokens x)));


let space? = (x) => (or (= x " ") (= x "\t"));
let paren? = (x) => match x
    	     	    | (in '["[", "]", "(", ")"]) => true,
		    | _ => false;

let operator? = (x) => (= (typeof (stoid x)) "operator");


let tokens+blanks = (s) => {
    let aux = (l acc w in_s) =>
    	match l
	| '[] => { cond | (= w "") => acc,
	      	   	| _ => (++ acc w); },
	| (cons h t) => { cond | in_s => { cond
	  	     	       	      	   | (= $h '"') =>
	  	     	       	      	       (aux $t acc (s+ w $h) false),
					   | _ => (aux $t acc (s+ w $h) true); },
			       | (space? $h) => (aux $t (++ acc $h) "" false),
	  	     	       | _ => (aux $t acc (s+ w $h) false); };

    aux (stol s) '[] "" false;
}


let map = (f l) => {
    let aux = (f l app) =>
    	match l
    	  | '[] => app,
    	  | (cons h t) => (aux f $t (++ app (f $h)));
    aux f l '[];
}

let re:visit = (s) => {
    let aux = (s) =>
    	cond
    	| (= (kind s) "number") => (overwrite s $re_color_green),
    	| (paren? s) => (overwrite s $re_color_yellow),
    	| (operator? s) => (overwrite s $re_color_purple),
    	| (= (kind s) "string") => (overwrite s $re_color_cyan),
    	| _ => (overwrite sentence $re_color_blank);
    print "aux = " (tos $aux) "\n";
    ltos (map $aux (tokens+blanks s));
}

let re:handle-backspace = (s l len ins c) =>
    cond
    | (< 0 ins) => { let del = (delete l (- ins 1));
      	   	     let ns = (ltos $del);
		     let fmt = (re:visit $ns);
		     let diff = (- len ins);
		     cond | (< 0 $diff) => (print (curs_bwd $diff));
		     '[$ns $del (- len 1) (- ins 1)]; },
    | _ => '[s l len ins];

let re:handle-printable = (s l len ins c) => {
    let ns = (ltos (insert l ins c));
    let fmt = (re:visit $ns);
    print (curs_bwd (- len ins));
    return '[$ns (stol $ns) (+ len 1) (+ ins 1)];
}

let re:handle-left = (a b c d e) =>
    cond
    | (< 0 d) => { print (curs_bwd 1); '[a b c (- d 1)]; },
    | _ => '[a b c d];

let re:handle-right = (a b c d e) =>
    cond
    | (< d c) => { print (curs_fwd 1); '[a b c (+ d 1)]; },
    | _ => '[a b c d];

let re:handle-undefined = (a b c d e) => '[a b c d e];

let re:handle-down = (a b c d e) => '[a b c d e];

let re:handle-up = (a b c d e) => '[a b c d e];

let re:handle-newline = (a b c d e) => a;

let keypress-dispatch = (c) =>
	cond
      | (= c "\n") => $re:handle-newline,
	  | (= c "A") => $re:handle-up,
	  | (= c "B") => $re:handle-down,
	  | (= c "C") => $re:handle-right,
	  | (= c "D") => $re:handle-left,
	  | (= c "undefined") => $re:handle-undefined,
	  | (or (= (chtoi c) 8) (= (chtoi c) 127)) =>
		$re:handle-backspace,
	  | _ => $re:handle-printable;

let re:repl = () => {
    let aux = (a b c d) => {
    	flush;
    	let ch = (getch);
	    let f = (keypress-dispatch $ch);
	    let ret = match (f a b c d $ch)
	      | '[a _ c d] => (aux $a $_ $c $d),
	      | _ => $_;
		return $ret;
    }
    rawmode;
    let r = (aux "" '[] 0 0);
    cookedmode;
    print "r is " $r "\n";
    return $r;
}

let re:default-prompt = () => (s+ (get PWD) "> ");

let re:display = (p) => cond
    	      	       	  | (!= p false) => { print p; flush; },
		       	          | _ => { print (re:default-prompt); flush; };

let loop = (f) => { f; loop f; }

let re:start-repl = () =>
    cond
      | (defined prompt) => (loop () => { print "\n";
				       	                  re:display (prompt);
										  eval (re:repl); }),
      | _ => (loop () => { print "\n";
      	   	     	       re:display (re:default-prompt);
			               eval (re:repl); });


re:start-repl;
