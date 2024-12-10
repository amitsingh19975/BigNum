from pathlib import Path
import os
from typing import List, Optional
import subprocess
from dataclasses import dataclass
import argparse
from random import randint, seed

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

def random_number(base: int, length: int) -> str:
    prefix = ''
    if base == 2:
        prefix = '0b'
    elif base == 8:
        prefix = '0o'
    elif base == 16:
        prefix = '0x'
    return prefix + ''.join([DIGITS[randint(0, base - 1)] for _ in range(length)])
        

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

def print_success(num: str, time: str) -> None:
    t_num = num
    if len(t_num) > 100:
        t_num = t_num[:100] + '...'

    print(f"{t_num} -- \x1b[32mPASSED\x1b[0m\n\tTook {time}")

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

def test_parse(max_len: int) -> None:
    path = get_bin_path()
    len = 1
    seed("BigNum")
    while True:
        len *= 10 #randint(1, max_len)
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


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog="Fuzzer")
    parser.add_argument('-c', '--parse', action='store_false', help="Fuzzy test integer parsing.");

    return parser.parse_args();

def main() -> None:
    args = parse_args();
    if args.parse:
        test_parse(10_000_000)

if __name__ == "__main__":
    main()

