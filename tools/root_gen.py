import argparse
from typing import  List, Tuple
import primefac
from math import log2, ceil

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog="NTT Root Generator");

    parser.add_argument('-b', '--bits', type=int, help="Number of bits required to represent max integer.", required=True)
    parser.add_argument('-p', '--prime', type=int, help="Prime number", default=150488372227)
    parser.add_argument('-t', '--test', help="Tests the root", action='store_true')

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

def validate_root(bits: int, root: int, mod: int) -> None:
    for i in range(bits + 1):
        n = 1 << i  # Current length to test
        k = (mod - 1) // n
        w = bin_pow(root, k, mod)
        iw = bin_pow(w, mod - 2, mod)
        
        print(f"\nTesting for length {n}:")
        print(f"{root=}, {iw=}, {k=}, {mod=}")
        
        # Test 1: w^n ≡ 1 (mod p)
        power = bin_pow(w, n, mod)
        if power != 1:
            print(f"FAILED: w^{n} ≡ {power} (mod {mod}), should be 1")
            continue
            
        # Test 2: w^k ≢ 1 for all k < n
        is_primitive = True
        sum = 1
        for j in range(1, n):
            pow = bin_pow(w, j, mod)
            if pow == 1:
                print(f"FAILED: w^{j} ≡ 1 (mod {mod}), shouldn't happen for j < {n}")
                is_primitive = False
                break
            sum += pow
        sum %= mod
        if sum != 0:
            print(f"FAILED: sum(w^j) != 0")
        if is_primitive:
            print(f"PASSED: Valid {n}th root of unity")
        
        # Optional: print the actual root for this length
        print(f"Root for length {n}: {w}")

def main() -> None:
    args = parse_args()

    bits = args.bits
    p = args.prime
    p, bits = fix_size(p, bits)
    n = p * (1 << bits) + 1
    root = generate_root(n)
    print(f"Root = {root} for MOD = {n}, Prime: {p}, Bits Required: {log2(n)}, Max Value = {(1 << (bits))-1}")
    if (args.test):
        validate_root(int(ceil(log2(n))), root, n)

if __name__ == "__main__":
    main()
