let c = (x) => cond
      	       | (= x 0) => (print "1) got 0\n"),
	       | (< x 5) => (print "2) got x < 5\n"),
	       | _ => (print "3) Unknown!\n");


c 0;
c 4;
c 10;