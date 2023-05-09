(load "highlight.re")

# (let curs_fwd "\e[1C")
# (let curs_bwd "\e[1D")
# for getting input from the user with a prompt

(let rest (l)
     (tl (length l) (tl (- (length l) 1) l)))

(let ltos-aux (l app)
     (if (= l [])
     	 app
	 (ltos-aux (rest l) (s+ app (first l)))))

(let ltos (l) (ltos-aux l ""))

(let gus_aux (app)
     (let cur (readch))
     (cond [(= (chtoi cur) 10) (print "\n") app]
	   [(or (= (chtoi cur) 127) (= (chtoi cur) 8))
	    (cond [(!= app "")
	    	   (print "\b \b")
		   (flush)])
	    (gus_aux (ltos (hd (- (length (stol app)) 1) (stol app))))]
	   [else (print cur) (flush) (gus_aux (s+ app cur))]))

(let get_user_string ()
     (gus_aux ""))

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