from typing import List, Tuple

BITS = 31
MAX_VALUE = (1<<BITS)
MASK = MAX_VALUE - 1

def convert(l: List[int]) -> int:
    a = 0
    for x in l[::-1]:
        a = a * MAX_VALUE + x

    return a

def toInternal(n: int) -> List[int]:
    a: List[int] = []
    n = abs(n)
    while n:
        v = n % MAX_VALUE
        a.append(v)
        n //= MAX_VALUE
    return a

def safe_add(out: List[int], b: List[int], offset: int, size: int) -> None:
    carry = 0
    for i in range(size):
        acc, c = safe_add_int(out[i + offset], b[i] + carry)
        out[i + offset] = acc
        carry = c

def safe_add_int(a: int, b: int) -> Tuple[int, int]:
    acc = a + b
    res = acc % MAX_VALUE
    carry = acc // MAX_VALUE
    return (res, carry)

def safe_sub_int(a: int, b: int, p: int) -> Tuple[int, int]:
    res = a - b - p
    if res < 0:
        return res + MAX_VALUE, 1
    return res, 0


def compare_arr(l: List[int], r: List[int]):
    if len(l) != len(r):
        print(f"Mismatch Len: {len(l)} != {len(r)}")

    size = min(len(l), len(r))
    print(f"{l=}\n{r=}\n")
    for i in range(size):
        if l[i] != r[i]:
            raise ValueError(f"Mismatch Value {i}: {l[i]} != {r[i]}")

def helper(lhs: List[int], rhs: List[int], size: int, depth: int = 0) -> Tuple[List[int], int]:
    if size <= 2:
        l = convert(lhs)
        r = convert(rhs)
        res = l * r
        return toInternal(res), -1 if res < 0 else 1

    half = size // 2
    low = half
    high = size - half
    print(f"size: {size}, half: {half}, low: {low}, high: {high}")

    # xl = [lhs[x] for x in range(0, low)]
    # xu = [lhs[x] for x in range(low, size)]
    # yl = [rhs[x] for x in range(0, low)]
    # yu = [rhs[x] for x in range(low, size)]
    xl = lhs[:low]
    xu = lhs[low:]
    yl = rhs[:low]
    yu = rhs[low:]

    xs = toInternal(convert(xl) + convert(xu))
    ys = toInternal(convert(yl) + convert(yu))
    
    print(f"xl: {xl}\nxu: {xu}\nyl: {yl}\nyu: {yu}\nxs: {xs}\nys: {ys}")
    print(f"xl_size: {len(xl)}\nxu_size: {len(xu)}\nyl_size: {len(yl)}\nyu_size: {len(yu)}\nxs_size: {len(xs)}\nys_size: {len(ys)}")

    print("======= Z0 = xl * yl =========");
    (z0, s0) = helper(xl, yl, low, depth + 1)
    print("======= Z2 = xu * yu =========");
    (z2, s2) = helper(xu, yu, high, depth + 1)
    print("======= Z3 = x_sum * y_sum =========");
    (z3, s3) = helper(xs, ys, high, depth + 1)
    print("=========== End ===========");

    print(f"z0: {z0}\nz2: {z2}\nz3: {z3}")

    sz = high << 1;
    while len(z0) < sz:
        z0.append(0)

    while len(z2) < sz:
        z2.append(0)

    while len(z3) < sz:
        z3.append(0)

    # print(f"\n========{depth}==========");

    z1_t = convert(z0)*s0 + convert(z2)*s2;
    z1 = convert(z3)*s3 - z1_t
    s1 = -1 if z1 < 0 else 1
    z1 = toInternal(z1)

    # for i in range(len(z1_t)):
    #     print(f"Z1: {i} => {z1_t[i]}")

    for _ in range(0, low * 2):
        z2.insert(0, 0)

    for _ in range(low):
        z1.insert(0, 0)

    # print(f"O1: {o1}\nO2: {o2}\nO3: {o3}")

    out = convert(z2)*s2 + convert(z1)*s1 + convert(z0)*s0
    so = -1 if out < 0 else 1
    out = toInternal(out)
    print(f"O: {out}")
    # for i in range(len(out)):
    #     print(f"{i} => {out[i]}")

    return out, so

a = 285933257128949549255701316273566808167968248937185782458137126418048882129169908160636217166197230933551105951409995695217855486946742356375826343875864750692692370708818530563648325264739241121034079

b = 306148553827645126378519434018193318495011822711963727391209954020528493864535209911159790699446229795516566739272057174475304538951592019486676553726213677917762908009727837714458341572581842964938427

LHS = toInternal(a)
RHS = toInternal(b)

N = max(len(LHS), len(RHS));

N = N + N % 2
while (len(LHS) < N):
    LHS.append(0)

while (len(RHS) < N):
    RHS.append(0)

res, rs = helper(LHS, RHS, len(LHS))
print(f"LHS: {repr(LHS)}");
num = rs * convert(res)
ans = a * b
if ans != num:
    print(f"\nAns: {ans}")
print(f"{res}\nString: {num}")


