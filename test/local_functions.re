let f = (x) => {
    let g = (x) => + x 1;
    g x;
}

# let's try some recursive local functions too
let foo = (x) => {
    let bar = (x) => match x
    	      	     | (= 0) => 0,
		     | _ => (+ 1 (bar (- x 1)));
    bar x;
}

f 6;
foo 6;