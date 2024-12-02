import random
import os
from pathlib import Path
from typing import List

CURRENT_PATH = Path(os.path.realpath(__file__)) / ".." / ".." / "test" / "runtime"
NUM = 827394650391827364598273645982736459827364598273645982736459827364598273645982736459982
BITS = len(bin(NUM)[2:])
FILE_PATH = (CURRENT_PATH / f"bitwise_mock.hpp").resolve()

def write_content(path: Path, content: str) -> None:
    with open(path, "w") as f:
        f.write('#pragma once\n#include "mock.hpp"\n\n')
        f.write(content)

def append_content(path: Path, content: str) -> None:
    with open(path, "a") as f:
        f.write(content)

def write_shift(name: str, content: str) -> None:
    temp = f"""
struct {name} {{
    Mock mock{{}};

    {name}() {{
        {content}  
    }}

    void add_test(std::string_view in, std::string_view out, std::size_t shift) {{
        MockTest test = {{
            .input = std::string(in),
            .output = std::string(out),
            .args = {{}}
        }};

        test.add_arg("shift", {{ .val = shift }});
        mock.tests.emplace_back(std::move(test));
    }}
}};
    """ 
    append_content(FILE_PATH, temp)

def left_shift(tests: int = 10) -> None:
    contents: List[str] = [] 
    shifts: set[int] = set()
    for _ in range(tests):
        shift = random.randint(0, BITS - 1)
        while shift in shifts:
            shift = random.randint(0, BITS - 1)
            shifts.add(shift)
        shifts.add(shift)
        num = NUM << shift;
        contents.append(f'add_test("{NUM}", "{hex(num)}", {shift});')
    
    content_lines = "\n".join(contents)
    write_shift("LeftShiftMock", content_lines)

def right_shift(tests: int = 10) -> None:
    contents: List[str] = [] 
    shifts: set[int] = set()
    for _ in range(tests):
        shift = random.randint(0, BITS - 1)
        while shift in shifts:
            shift = random.randint(0, BITS - 1)
            shifts.add(shift)
        shifts.add(shift)
        num = NUM >> shift;
        contents.append(f'add_test("{NUM}", "{hex(num)}", {shift});')
    
    content_lines = "\n".join(contents)
    write_shift("RightShiftMock", content_lines)

def main() -> None:
    random.seed(101)
    write_content(FILE_PATH, "")
    left_shift()
    right_shift()

if __name__ == "__main__":
    main()
