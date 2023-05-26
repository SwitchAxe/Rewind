(let re_color_black "\e[0;30m")
(let re_color_red "\e[0;31m")
(let re_color_green "\e[0;32m")
(let re_color_orange "\e[0;33m")
(let re_color_blue "\e[0;34m")
(let re_color_purple "\e[0;35m")
(let re_color_cyan "\e[0;36m")
(let re_color_lgray "\e[0;37m")
(let re_color_dgray "\e[1;30m")
(let re_color_lred "\e[1;31m")
(let re_color_lgreen "\e[1;32m")
(let re_color_yellow "\e[1;33m")
(let re_color_lblue "\e[1;34m")
(let re_color_lpurple "\e[1;35m")
(let re_color_lcyan "\e[1;36m")
(let re_color_white "\e[1;37m")
(let re_color_blank "\e[0m")
(let re_color_attr_bold "\e[1m")

# cursor movement forward/backwards
(let curs_fwd "\e[1C")
(let curs_bwd "\e[1D")

# erase the current line, starting from the cursor
(let curs_erase "\e[0K")

# arrow key codes to be caught by readch
(let arr_up "\e[A")
(let arr_down "\e[B")
(let arr_right "\e[C")
(let arr_left "\e[D")

(let rest (l)
     (tl (length l) (tl (- (length l) 1) l)))

(let restn (l n)
     (tl (length l) (tl (- (length l) n) l)))

#map that takes a procedure which in turn doesn't take arguments
#used only for side effects, see below.

#note: tail recursive
(let map-noargs (f l dummy)
     (if (= l [])
     	 true
	 (map-noargs f (rest l) (f))))

(let range (inf sup)
     (cond [(< sup inf) []]
     	   [(< inf sup) (++ inf (range (+ inf 1) sup))]
	   [else []]))

(let escape_sequences [["cursor up" 91 65]
     		       ["cursor down" 91 66]
		       ["cursor right" 91 67]
		       ["cursor left" 91 68]])

(let getch ()
     (let fst_b (readch))
     (if (!= (chtoi fst_b) 27)
     	 fst_b
	 (match (chtoi (readch))
	     	[(= 91) (match (chtoi (readch))
	     		       [(= 65) "up"]
	     	    	       [(= 66) "down"]
		    	       [(= 67) "right"]
		    	       [(= 68) "left"]
		    	       [_ "undefined"])]
		[(_) "undefined"])))

(let overwrite (word color)
     (print color word re_color_blank))

(let kind (x) (typeof (ast x)))

(let map-aux (f l app)
     (cond [(= l []) app]
     	   [else (map-aux f (rest l) (++ app (f (first l))))]))

(let map (f l)
     (map-aux f l []))

(let accumulate (f l app)
     (cond [(= l []) app]
     	   [else (accumulate f (rest l) (f (first l) app))]))

(let min (a b) (if (< a b) a b))
(let max (a b) (if (< a b) b a))

(let number? (x) (= (typeof (ast x)) "number"))
(let list? (x) (= (typeof (ast x)) "list"))
(let string? (x) (= (typeof (ast x)) "string"))
(let paren? (x) (or (= x "[") (= x "]")
     	    	    (= x "(") (= x ")")))

(let space? (x) (or (= x " ") (= x "\t")))

(let ws_split-aux (l acc app)
     (cond [(= l []) (if (= acc "") app (++ app acc))]
     	   [(space? (first l)) (if (= acc "")
	   	    	       	   (ws_split-aux (rest l) "" app)
				   (ws_split-aux (rest l) "" (++ app acc)))]
	   [else (ws_split-aux (rest l) (s+ acc (first l)) app)]))

(let ws_split (s)
     (ws_split-aux (stol s) "" []))

(let format_list-aux (l)
     (cond [(= l []) []]
	   [(= (rest l) []) (first l)]
	   [(= (typeof (first l)) "list")
	    (format_list (first l))]
     	   [else (++ (first l) " " (format_list-aux (rest l)))]))

(let format_list (s)
     (++ "[" (format_list-aux (ast s)) "]"))

# visits the AST of 'sentence' and highlights it
(let visit (sentence)
     (cond [(or (= sentence "(") (= sentence "["))
            (overwrite sentence re_color_yellow)
	    sentence]
	   [(or (= sentence ")") (= sentence "]"))
	    (overwrite sentence re_color_yellow)
	    sentence]
	   [(or (= sentence " ") (= sentence "\t"))
	    (print sentence)
	    sentence]
	   [(= (kind sentence) "operator")
     	    (overwrite sentence re_color_blue)
	    sentence]
	   [(= (kind sentence) "list")
	    (let as_l (stol sentence))
	    (ltos (map (let (e) (visit (tos e))) as_l))]
	   [(= (kind sentence) "number")
	    (overwrite sentence re_color_green)
	    sentence]
	   [(= (kind sentence) "error")
	    (overwrite sentence re_color_red)
	    sentence]
	   [(= (kind sentence) "identifier")
	    (overwrite sentence re_color_purple)
	    sentence]
	   [else
	    (overwrite sentence re_color_blank)
	    sentence]))

(let find (l i)
     (cond [(< i 0) []]
     	   [else (first (tl 1 (hd (+ i 1) l)))]))

(let gus_aux (app as_list len inspos)
     (flush)
     (let cur (getch))
     (cond [(= cur "left")
	    (cond [(< 0 inspos)
	    	   (print curs_bwd)
		   (flush)
		   (gus_aux app as_list len (- inspos 1))]
		  [else (gus_aux app as_list len inspos)])]
	   [(= cur "right")
	    (cond [(< inspos len)
	    	   (print curs_fwd)
		   (flush)
		   (gus_aux app as_list len (+ inspos 1))]
		  [else (gus_aux app as_list len inspos)])]
	   [(or (= cur "up") (= cur "down") (= cur "undefined"))
	    (gus_aux app as_list len inspos)]
	   [(= (chtoi cur) 10) (print "\n") app]
	   [(or (= (chtoi cur) 127) (= (chtoi cur) 8))
	    (cond [(< 0 inspos)
		   (let del (delete as_list (- inspos 1)))
		   (let new_app (ltos del))
		   (print "\e[" inspos "D" curs_erase)
		   (let formatted (visit new_app))
	    	   (let diff (- (- len 1) (- inspos 1)))
	    	   (cond [(< 0 diff) (print "\e[" diff "D")])
	    	   (flush)
		   (gus_aux new_app
		  	    del
		  	    (- len 1)
			    (- inspos 1))]
		  [else (gus_aux app
		  		 as_list
				 len
				 0)])]
	   [else
	    (let new_app (ltos (insert as_list cur inspos)))
	    (if (< 0 inspos) (print "\e[" inspos "D" curs_erase) false)
	    (let formatted (visit new_app))
	    (let new_as_list (stol new_app))
	    (let diff (- (+ len 1) inspos))
	    (cond [(< 1 diff) (print "\e[" diff "D" curs_fwd)])
	    (flush)
	    (gus_aux formatted new_as_list (+ len 1)
		     (+ 1 inspos))]))

(let get_user_string ()
     (rawmode)
     (let res (gus_aux "" [] 0 0))
     (cookedmode)
     res)

(let get_input (p)
     (cond [(!= p false)
     	    (print p)
	    (flush)
     	    (get_user_string)]
	   [else (print (s+ (get PWD) "> "))
	   	 (get_user_string)]))

(let evloop (fun) (fun) (evloop fun))

(if (defined prompt)
    (evloop (let () (print (eval (get_input (prompt))))))
    (evloop (let () (print (eval (get_input false))))))