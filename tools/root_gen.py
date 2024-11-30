import argparse
from typing import List, Tuple
import primefac
from math import log2, ceil

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog="NTT Root Generator");

    parser.add_argument('-b', '--bits', type=int, help="Number of bits required to represent max integer.", required=True)
    parser.add_argument('-p', '--prime', type=int, help="Prime number", default=150488372227)

    return parser.parse_args()

def prime_factors(n: int) -> List[int]:
    res: List[int] = []

    gen = primefac.primefac(n)
    for i in gen:
        res.append(i)


    return res

def euler_totient(n: int) -> int:
    facts = prime_factors(n)
    for f in facts:
        n = n // f
        n *= (f - 1)
    return n

def bin_pow(n: int, p: int, mod: int) -> int:
    res = 1
    while p:
        if (p & 1):
            res = (res * n) % mod
        n = (n * n) % mod
        p >>= 1
    return res

def generate_root(n: int) -> int:
    phi = euler_totient(n)
    factors = prime_factors(phi)

    for root in range(2, n):
        found = True
        for d in factors:
            pow = bin_pow(root, int(phi // d), n)
            if pow == 1:
                found = False
                break
        if found:
            return root

    return 1 if n == 1 else -1

def fix_size(p: int, bits: int) -> Tuple[int, int]:
    cal_size = lambda p, bits: p * (1 << bits) + 1

    n = cal_size(p, bits)
    if primefac.isprime(n):
        return p, bits
    gen = primefac.primegen()
    for x in gen:
        if p > x:
            continue
        break

    i = 0

    while i < 10000:
        p = next(gen)
        n = cal_size(p, bits)
        if not primefac.isprime(n):
            continue
        print(f"{ceil(log2(n))=} == {bits=}")
        bs = ceil(log2(n))
        if bs >= bits:
            break
        i += 1
        

    return p, bits

    

def main() -> None:
    args = parse_args()
    bits = args.bits
    p = args.prime
    p, bits = fix_size(p, bits)
    n = p * (1 << bits) + 1
    print(f"Root = {generate_root(n)} for MOD = {n}, Prime: {p}, Bits Required: {log2(n)}, Max Value = {(1 << (bits))-1}")

if __name__ == "__main__":
    main()
