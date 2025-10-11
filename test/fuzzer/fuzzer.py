from pathlib import Path
import os
from typing import List, Optional
import subprocess
from dataclasses import dataclass
import argparse
from random import randint, seed

import sys
sys.set_int_max_str_digits(0)

CURRENT_PATH = Path(os.path.realpath(__file__)) / '..'
SHARE_FILE = (CURRENT_PATH / 'shared.txt').resolve()
ERROR_INPUT = (CURRENT_PATH / 'error.txt').resolve()

def get_bin_path(name: str = "driver") -> Path:
    return CURRENT_PATH / name

@dataclass
class Result:
    success: Optional[str] = None
    error: Optional[str] = None

DIGITS = "0123456789abcdef"

def random_number(base: int, length: int, is_neg: bool = False) -> str:
    prefix = ''
    if base == 2:
        prefix = '0b'
    elif base == 8:
        prefix = '0o'
    elif base == 16:
        prefix = '0x'
    s = ''
    if is_neg:
        s = '-'
    n = [DIGITS[randint(0, base - 1)] for _ in range(length)]
    while n[0] == '0':
        n[0] = DIGITS[randint(0, base - 1)]

    return s + prefix + ''.join(n)

def run_bin(path: Path, args: List[str]) -> Result:
    temp_args = [str(path.resolve())] + args;

    res = subprocess.run(temp_args, capture_output=True, text=True);
    if (res.stderr):
        return Result(error=res.stderr)

    if res.stdout:
        return Result(success=res.stdout)

    return Result();

def read_lines() -> List[str]:
    with open(SHARE_FILE, 'r') as f:
        return f.readlines()

def print_error(num: str, err: str) -> None:
    t_num = num
    if len(t_num) > 100:
        t_num = t_num[:100] + '...'
    with open(ERROR_INPUT, 'w') as f:
        f.write(num)
    print(f"{t_num} -- \x1b[31mFAILED\x1b[0m\n\t{err}")

def print_binary_error(a: str, b: str, ans: str, op: str, err: str) -> None:
    t_a = a
    t_b = b
    if len(t_a) > 50:
        t_a = t_a[:50] + '...'
    if len(t_b) > 50:
        t_b = t_b[:50] + '...'
    with open(ERROR_INPUT, 'w') as f:
        f.write(a)
        f.write('\n')
        f.write(b)
        f.write('\n')
        f.write(ans)
    o = op
    if op == 'a':
        o = '+'
    elif op == 's':
        o = '-'
    elif op == 'm':
        o = '*'
    print(f"{t_a} {o} {t_b} -- \x1b[31mFAILED\x1b[0m\n\t{err}")

def print_success(num: str, time: str) -> None:
    t_num = num
    if len(t_num) > 100:
        t_num = t_num[:100] + '...'

    print(f"{t_num} -- \x1b[32mPASSED\x1b[0m\n\tTook {time}")

def print_binary_success(a: str, b: str, op: str, time: str) -> None:
    t_a = a
    t_b = b
    if len(t_a) > 50:
        t_a = t_a[:50] + '...'
    if len(t_b) > 50:
        t_b = t_b[:50] + '...'

    o = op
    if op == 'a':
        o = '+'
    elif op == 's':
        o = '-'
    elif op == 'm':
        o = '*'
    print(f"{t_a} {o} {t_b} -- \x1b[32mPASSED\x1b[0m\n\tTook {time}")

PREFIX_LIST = ['0b', '0o', '0x']

def compare_num(lhs: str, rhs: str) -> bool:
    if lhs[:2] in PREFIX_LIST:
        lhs = lhs[2:]
        rhs = rhs[2:]

    lhs = lhs.lstrip('0')
    rhs = rhs.lstrip('0')

    if len(lhs) != len(rhs):
        print(f"Mismatch Len: {len(lhs)} != {len(rhs)}")
        return False

    for i in range(len(lhs)):
        if lhs[i] != rhs[i]:
            print(f"Mismatch at {i}: '{lhs[i]}' != '{rhs[i]}'");
            return False

    return True

def test_parse_helper(path: Path, num: str, use_shared = True) -> bool:
    args = ['-c']
    if use_shared:
        with open(SHARE_FILE, 'w') as f:
            f.write(num)
        args.append('-f')
        args.append(str(SHARE_FILE))
    else:
        args.append(num)

    res = run_bin(path, args)
    if res.error:
        print_error(num, res.error);
        return False

    lines = read_lines()
    if (len(lines) < 2):
        print_error(num, "Expected file to have number and time, but found invalid lines");
        return False

    out = lines[0].strip();
    time = lines[1];
    if (not compare_num(out, num)):
        print_error(num, "Mismatch input and output");
        if (res.success and res.success.strip()):
            lines = res.success.splitlines()
            out = ''.join([f"\t> {line}\n" for line in lines])
            print(f"{out}\n");
        return False

    print_success(num, time)
    return True

def test_binary_helper(path: Path, a: str, b: str, ans: str, op: str, use_shared = True) -> bool:
    args = [f'-{op}']
    if use_shared:
        with open(SHARE_FILE, 'w') as f:
            f.write(a)
            f.write('\n')
            f.write(b)
        args.append('-f')
        args.append(str(SHARE_FILE))
    else:
        args.append(a)
        args.append(b)

    res = run_bin(path, args)
    if res.error:
        print_binary_error(a, b, ans, op, res.error);
        return False

    lines = read_lines()
    if (len(lines) < 2):
        print_binary_error(a, b, ans, op, "Expected file to have number and time, but found invalid lines");
        return False

    out = lines[0].strip();
    time = lines[1];
    if (not compare_num(out, ans)):
        print_binary_error(a, b, ans, op, "Mismatch input and output");
        if (res.success and res.success.strip()):
            lines = res.success.splitlines()
            out = ''.join([f"\t> {line}\n" for line in lines])
            print(f"{out}\n");
        return False

    print_binary_success(a, b, op, time)
    return True

def test_parse(max_len: int) -> None:
    path = get_bin_path()
    len = 1
    seed("BigNum")
    while True:
        len +=100 # randint(1, max_len)
        print(f"Testing for number that has length: {len}")
        
        num = random_number(2, len)
        if not test_parse_helper(path, num):
            break

        num = random_number(8, len)
        if not test_parse_helper(path, num):
            break

        num = random_number(10, len)
        if not test_parse_helper(path, num):
            break

        num = random_number(16, len)
        if not test_parse_helper(path, num):
            break

def to_string(n: int, base: int) -> str:
    match base:
        case 2: return bin(n)
        case 8: return oct(n)
        case 16: return hex(n)
        case _: return str(n)

def test_binary(max_len: int, op='a') -> None:
    path = get_bin_path()
    len1 = 1
    len2 = 1
    base = 2
    seed("BigNum")
    while True:
        len1 += 100 #randint(1, max_len)
        len2 += 100 #randint(1, max_len)
        print(f"Testing for number that has length: {len1=}, {len2=}")

        a_neg = True if randint(0, 10) > 5 else False
        b_neg = True if randint(0, 10) > 5 else False
        a = random_number(base, len1, a_neg)
        b = random_number(base, len2, b_neg)
        ans = 0
        na = int(a, base)
        nb = int(b, base)
        if op == 'a':
            ans = na + nb
        elif op == 's':
            ans = na - nb
        elif op == 'm':
            ans = na * nb
        ans = to_string(ans, base)
        if not test_binary_helper(path, a, b, ans, op):
            break

        # num = random_number(16, len)
        # if not test_parse_helper(path, num):
        #     break


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog="Fuzzer")
    parser.add_argument('-c', '--parse', action=argparse.BooleanOptionalAction, help="Fuzzy test integer parsing.");
    parser.add_argument('-b', '--binary', help="Fuzzy test binary operation.", choices=['a', 's', 'm']);

    return parser.parse_args();

def main() -> None:
    args = parse_args();
    if args.parse:
        test_parse(10_000_000)
    elif args.binary:
        test_binary(10_000, op = args.binary)

if __name__ == "__main__":
    main()

