let F = () => { print "success! (1)\n"; }
let G = (f) => (f);
let H = () => (G () => print "success! (2)\n";);

F;
G F;
H;