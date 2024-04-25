let map = (l f) => match l
    	      	 | '[] => '[],
    	      	 | (cons h t) => (++ (f $h) (map $t f));

print (map '[1 2 3 4 5] (x) => (+ x 1));