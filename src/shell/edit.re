# (load highlight.re)

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


(print re_color_blank)

#(let curs_fwd "\e[1C")
#(let curs_bwd "\e[1D")
# for getting input from the user with a prompt

(let rest (l)
     (tl (length l) (tl (- (length l) 1) l)))

(let ltos-aux (l app)
     (if (= l [])
     	 app
	 (ltos-aux (rest l) (s+ app (first l)))))

(let ltos (l) (ltos-aux l ""))

# for overwriting a word with a colored variant depending
# on what kind of semantic element it represents.

(let map (f l)
     if (= l [])
     	[]
	(++ (f (first l)) (map f (rest l))))

(let range (inf sup)
     (if (= inf sup)
     	 sup
	 (++ (range (+ inf 1) sup) inf)))

(let iter (idx limit fun)
     (map fun (range idx limit)))

(let overwrite (word color)
     (let as_list (stol word))
     (let word_length (length as_list))
     (iter 1 word_length (let () (print "\b")))
     (print color word re_color_blank)
     (flush))

(let gus_aux (app lastw)
     (let cur (readch))
     (cond [(= (chtoi cur) 10) (print "\n") app]
	   [(or (= (chtoi cur) 127) (= (chtoi cur) 8))
	    (cond [(!= app "")
	    	   (print "\b \b")
		   (flush)])
	    (gus_aux (ltos (hd (- (length (stol app)) 1) (stol app)))
	    	     (ltos (hd (- (length (stol lastw)) 1) (stol lastw))))]
	   [else (print cur)
	   	 (cond [(= (typeof (eval (s+ lastw cur))) "error")
		        (overwrite (s+ lastw cur) re_color_red)]
		       [(defined (s+ lastw cur)) (overwrite lastw re_color_yellow)]
		       [(= (typeof (eval (s+ lastw cur))) "list")
		       	(overwrite (s+ lastw cur) re_color_attr_bold)]
		       [(= (typeof (eval (s+ lastw cur))) "operator")
		       	(overwrite (s+ lastw cur) re_color_green)]
		       [(= (typeof (eval (s+ lastw cur))) "string")
		        (overwrite (s+ lastw cur) re_color_cyan)])
	   	 (flush)
		 (if (or (= (chtoi cur) 32) (= (chtoi cur) 9))
		     (gus_aux (s+ app cur) "")
		     (gus_aux (s+ app cur) (s+ lastw cur)))]))

(let get_user_string ()
     (gus_aux "" ""))

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