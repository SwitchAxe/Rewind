let g = (x) => {
    let a = + x 3;
    let b = + $a 2;
    return $b;
}

let f = (x) => {
    let y = + x 1;
    let z = * $y 2;
    return $z;
}
g 5;
f 5;