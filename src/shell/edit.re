let re_color_black "\e[0;30m";
let re_color_red "\e[0;31m";
let re_color_green "\e[0;32m";
let re_color_orange "\e[0;33m";
let re_color_blue "\e[0;34m";
let re_color_purple "\e[0;35m";
let re_color_cyan "\e[0;36m";
let re_color_lgray "\e[0;37m";
let re_color_dgray "\e[1;30m";
let re_color_lred "\e[1;31m";
let re_color_lgreen "\e[1;32m";
let re_color_yellow "\e[1;33m";
let re_color_lblue "\e[1;34m";
let re_color_lpurple "\e[1;35m";
let re_color_lcyan "\e[1;36m";
let re_color_white "\e[1;37m";
let re_color_blank "\e[0m";
let re_color_attr_bold "\e[1m";

# cursor movement forward/backwards
let curs_fwd "\e[1C";
let curs_bwd "\e[1D";

# erase the current line, starting from the cursor
let curs_erase "\e[0K";

#map that takes a procedure which in turn doesn't take arguments
#used only for side effects, see below.

#note: tail recursive
let map-noargs (f l dummy)
    | = l [] => true,
    | _ => map-noargs f (rest l) (f);


let parse_arrow_key (ch)
    | = ch "A" => "up",
    | = ch "B" => "down",
    | = ch "C" => "right",
    | = ch "D" => "left",
    | _ => "undefined";

let getch ()
    let fst_b (readch),
    | != (chtoi $fst_b) 27 => @$fst_b,
    | != (chtoi (readch)) 91 => "undefined",
    | _ => parse_arrow_key (readch);

let overwrite (word color)
    print color word $re_color_blank,
    @word;

let kind (x) typeof (ast x);


let if_then (p f)
    | = (p) true => f;


let map-aux (f l app)
    | = l [] => @app,
    | _ => map-aux f (rest l) (++ app (f (first l)));

let map (f l)
    map-aux f l [];

let number? (x) = (kind x) "number";
let list? (x) = (typeof x) "list";
let space? (x) or (= x " ") (= x "\t");
let string? (x) = (kind x) "string";
let paren? (x) ? @x | in ["[" "]" "(" ")"] => true, | _ => false;
let operator? (x) = (typeof (stoid x)) "operator";


let visit_dispatch (sentence)
    | space? sentence => overwrite sentence $re_color_blank,
    | number? sentence => overwrite sentence $re_color_green,
    | string? sentence => overwrite sentence $re_color_cyan,
    | paren? sentence => overwrite sentence $re_color_yellow,
    | operator? sentence => overwrite sentence $re_color_purple,
    | _ => overwrite sentence $re_color_blank;


let tokens+spaces-aux (l tks app in_s sl)
    | and (= l []) (!= sl []) => ++ app (ltos sl),
    | = tks [] => ++ app l,
    | and (= (first l) '"') in_s => tokens+spaces-aux (rest l)
                                                      (rest tks)
                                                      (++ app
                                                       (s+ (ltos sl)
                                                           '"'))
                                                      false
                                                      [],
    | = (first l) '"' => tokens+spaces-aux (rest l)
                                           tks
                                           app
                                           true
                                           ['"'],
    | @in_s => tokens+spaces-aux (rest l) tks app in_s (++ sl (first l)),
    | space? (first l) => tokens+spaces-aux (rest l)
                                            tks
                                            (++ app (first l))
                                            false
                                            [],
    | _ => tokens+spaces-aux (rest l (length (stol (first tks))))
      	   		             (rest tks)
			                 (++ app (first tks))
                             false
                             [];

let tokens+spaces (s tks)
    tokens+spaces-aux (stol s) tks [] false [];

let visit (s)
    ltos (map visit_dispatch (tokens+spaces s (tokens s)));


let handle_backspace_keypress (app as_l len ins cur)
    | < 0 ins => let del delete as_l (- ins 1),
      	      	 let new_app ltos $del,
		         print "\e[" ins "D" $curs_erase,
		         let formatted visit $new_app,
		         let diff - len ins,
		         if_then () => < 0 $diff; () => print "\e[" $diff "D";,
		         flush,
		         $new_app $del (- len 1) (- ins 1), 
    | _ => app as_l len ins;

let handle_printable_keypress (app as_l len ins cur)
    let new_app ltos (insert as_l cur ins),
    let ins @ins,
    if_then () => < 0 $ins; () => print "\e[" $ins "D" $curs_erase;,
    visit $new_app,
    let new_as_l stol $new_app,
    let diff - (+ len 1) ins,
    if_then () => < 1 $diff; () => print "\e[" $diff "D" $curs_fwd;,
    flush,
    [$new_app $new_as_l (+ len 1) (+ ins 1)];

let handle_left_arrow_keypress (app as_l len ins cur)
    | < 0 ins => print $curs_bwd, flush, app as_l len (- ins 1),
    | _ => app as_l len ins;

let handle_right_arrow_keypress (app as_l len ins cur)
    | < ins len => print $curs_fwd, flush, app as_l len (+ ins 1),
    | _ => app as_l len ins;

# unused
let handle_undefined_keypress (app as_l len ins cur)
    ;

# unused
let handle_down_arrow_keypress (app as_l len ins cur)
    ;

# unused
let handle_up_arrow_keypress (app as_l len ins cur)
    ;

let handle_newline_keypress (app as_l len ins cur)
    @app;

let keypress_dispatch_function (ch)
    | = ch "\n" => @handle_newline_keypress,
    | = ch "left" => @handle_left_arrow_keypress,
    | = ch "right" => @handle_right_arrow_keypress,
    | = ch "up" => @handle_up_arrow_keypress,
    | = ch "down" => @handle_down_arrow_keypress,
    | = ch "undefined" => @handle_undefined_keypress,
    | or (= (chtoi ch) 8) (= (chtoi ch) 127) => @handle_backspace_keypress,
    | _ => @handle_printable_keypress;

let get_user_string_aux (app as_l len ins)
    let ch getch,
    let fun keypress_dispatch_function $ch,
    ? $fun app as_l len ins $ch
    | [a * c d] => get_user_string_aux $a $* $c $d,
    | _ => @$_;

let get_user_string ()
    rawmode,
    let res get_user_string_aux "" [] 0 0,
    cookedmode,
    @$res;

let get_input (p)
    | @p => print p, flush, get_user_string,
    | _ => print (s+ (get PWD) "> "), get_user_string;

let evloop (fun) fun, evloop fun;

| defined prompt => evloop () => print "\n" (eval << get_input << prompt) "\n";,
| _ => evloop () => print "\n" (eval << get_input false) "\n";