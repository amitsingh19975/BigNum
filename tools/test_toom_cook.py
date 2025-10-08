from basic import Internal

def helper(lhs: Internal, rhs: Internal, size: int, depth: int = 0) -> Internal:
    if size <= 2:
        return lhs * rhs

    left_size = size // 3
    mid_size = left_size * 2



    ll = lhs[:left_size]
    lm = lhs[left_size:mid_size]
    lr = lhs[mid_size:]

    rl = rhs[:left_size]
    rm = rhs[left_size:mid_size]
    rr = rhs[mid_size:]
    # print(f"\n\n================= {depth} =================\n");
    print(f"ll: {repr(ll)}\nlm: {repr(lm)}\nlr: {repr(lr)}");
    print(f"rl: {repr(rl)}\nrm: {repr(rm)}\nrr: {repr(rr)}");

    l_n2 = Internal()
    l_n1 = Internal()
    l_0  = Internal()
    l_1  = Internal()
    l_inf= Internal()


    r_n2 = Internal()
    r_n1 = Internal()
    r_0  = Internal()
    r_1  = Internal()
    r_inf= Internal()

    def eval(m0: Internal, m1: Internal, m2: Internal):
        pt = m0 + m2
        p_0 = m0
        p_1 = pt + m1
        p_n1 = pt - m1
        p_n2 = (p_n1 + m2) * 2 - m0
        p_inf = m2

        return p_n2, p_n1, p_0, p_1, p_inf

    l_n2, l_n1, l_0, l_1, l_inf = eval(ll, lm, lr)
    r_n2, r_n1, r_0, r_1, r_inf = eval(rl, rm, rr)

    ls = '' if l_n2.is_neg else '+'
    rs = '' if r_n2.is_neg else '+'
    print(f"\n\nln2: {ls}{repr(l_n2)}\nln1: {repr(l_n1)}\nl0: {repr(l_0)}\nl1: {repr(l_1)}\nl_inf: {repr(l_inf)}\n")
    print(f"\n\nrn2: {rs}{repr(r_n2)}\nrn1: {repr(r_n1)}\nr0: {repr(r_0)}\nr1: {repr(r_1)}\nr_inf: {repr(r_inf)}\n")

    s1 = l_n2.is_neg
    s2 = r_n2.is_neg
    o_n2 =  helper(l_n2, r_n2, left_size, depth + 1)
    o_n2.is_neg = s1 != s2


    s1 = l_n1.is_neg
    s2 = r_n1.is_neg
    o_n1 =  helper(l_n1, r_n1, left_size, depth + 1)
    o_n1.is_neg = s1 != s2

    s1 = l_0.is_neg
    s2 = r_0.is_neg
    o_0 =   helper(l_0, r_0, left_size, depth + 1)
    o_0.is_neg = s1 != s2

    s1 = l_1.is_neg
    s2 = r_1.is_neg
    o_1 =   helper(l_1, r_1, left_size, depth + 1)
    o_1.is_neg = s1 != s2

    s1 = l_inf.is_neg
    s2 = r_inf.is_neg
    o_inf = helper(l_inf, r_inf, left_size, depth + 1)
    o_inf.is_neg = s1 != s2

    print("\non2: {}\non1: {}\no0: {}\no1: {}\noinf: {}\n".format(repr(o_n2), repr(o_n1), repr(o_0), repr(o_1), repr(o_inf)))

    o0 = o_0
    o4 = o_inf

    o3 = (o_n2 - o_1) // 3
    print(f"3: {repr(o3)}")

    o1 = (o_1 - o_n1) // 2
    print(f"4: {repr(o1)}")

    o2 = (o_n1 - o_0)
    print(f"5: {repr(o2)}")

    o3 = ((o2 - o3) // 2) + o_inf * 2
    print(f"6: {repr(o3)}")

    o2 = (o2 + o1 - o4)
    print(f"7: {repr(o2)}")

    o1 = o1 - o3;
    print(f"8: {repr(o1)}")

    print(f"o0: {repr(o0)}\no1: {repr(o1)}\no2: {repr(o2)}\no3: {repr(o3)}\no4: {repr(o4)}")

    for _ in range(4 * left_size):
        o4.rep.insert(0,0)

    for _ in range(3 * left_size):
        o3.rep.insert(0,0)

    for _ in range(2 * left_size):
        o2.rep.insert(0,0)

    for _ in range(1 * left_size):
        o1.rep.insert(0,0)

    # print(f"\nTMP: {repr(o1)} + {repr(o0)} =>\n\t {repr(o1 + o0)}")

    out = o4 + o3 + o2 + o1 + o0
    print(f"O: {repr(out)}")
    return out

a = 306148553827645126378519434018193318495011822711963727391209954020528493864535209911159790699446229795516566739272057174475304538951592019486676553726213677917762908009727837714458341572581842964938427

LHS = Internal(a)
RHS = Internal(a)

N = max(len(LHS), len(RHS));

N = N + (3 - N % 3)
while (len(LHS) < N):
    LHS.rep.append(0)

while (len(RHS) < N):
    RHS.rep.append(0)

res = helper(LHS, RHS, N)
assert(res.to_int() == a * a)
print(f"{res=}\nRes: {res}")


