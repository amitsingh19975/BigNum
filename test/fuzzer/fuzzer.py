from pathlib import Path
import os
from typing import List, Optional
import subprocess
from dataclasses import dataclass
import argparse
from random import randint

CURRENT_PATH = Path(os.path.realpath(__file__)) / '..'
SHARE_FILE = (CURRENT_PATH / 'shared.txt').resolve()

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

    return Result(error="<unknown state>");


def test_construction_helper(path: Path, num: str, use_shared = False) -> bool:
    args = ['-c']
    if use_shared:
        with open(SHARE_FILE, 'w') as f:
            f.write(num)
        args.append('-f')
        args.append(str(SHARE_FILE))
    else:
        args.append(num)

    res = run_bin(path, args)
    if len(num) > 100:
        num = num[:100] + '...'
    if res.error:
        print(f"{num} -- \x1b[31mFAILED\x1b[0m\n\t{res.error}")
        return False
    print(f"{num} -- \x1b[32mPASSED\x1b[0m\n\t{res.success}")
    return True

def test_construction(max_len: int) -> None:
    path = get_bin_path()
    while True:
        len = randint(1, max_len)
        print(f"Testing for number that has length: {len}")
        
        use_shared = True if len > 10_000 else False

        num = random_number(2, len)
        if not test_construction_helper(path, num, use_shared):
            break

        num = random_number(8, len)
        if not test_construction_helper(path, num, use_shared):
            break

        num = random_number(10, len)
        if not test_construction_helper(path, num, use_shared):
            break

        num = random_number(16, len)
        if not test_construction_helper(path, num, use_shared):
            break


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog="Fuzzer")
    parser.add_argument('-c', '--construction', action='store_false', help="Fuzzy test integer construction.");

    return parser.parse_args();

def main() -> None:
    args = parse_args();
    if args.construction:
        test_construction(10_000_000)

if __name__ == "__main__":
    main()

