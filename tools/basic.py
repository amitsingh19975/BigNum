from typing import List, Union

BITS = 31
MAX_VALUE = (1<<BITS)
MASK = MAX_VALUE - 1

class Internal:
    def __init__(self, n: Union[int, List[int]] = 0, is_neg = False) -> None:
        if n:
            if isinstance(n, int):
                self.is_neg = n < 0
                self.rep = Internal.to_internal(n) 
            else:
                self.is_neg = is_neg
                self.rep = n
        else:
            self.is_neg = is_neg
            self.rep = []

    @staticmethod
    def convert(l: List[int]) -> int:
        a = 0
        for x in l[::-1]:
            a = a * MAX_VALUE + x

        return a

    @staticmethod
    def to_internal(n: int) -> List[int]:
        a: List[int] = []
        n = abs(n)
        while n > 0:
            a.append((n % MAX_VALUE))
            n //= MAX_VALUE
        return a

    def to_int(self) -> int:
        n = Internal.convert(self.rep)
        return -n if self.is_neg else n

    def __add__(self, other) -> 'Internal':
        if isinstance(other, Internal):
            return Internal(self.to_int() + other.to_int())
        else:
            return Internal(self.to_int() + other)

    def __sub__(self, other) -> 'Internal':
        if isinstance(other, Internal):
            return Internal(self.to_int() - other.to_int())
        else:
            return Internal(self.to_int() - other)

    def __mul__(self, other) -> 'Internal':
        if isinstance(other, Internal):
            return Internal(self.to_int() * other.to_int())
        else:
            return Internal(self.to_int() * other)

    def __mod__(self, other) -> 'Internal':
        if isinstance(other, Internal):
            return Internal(self.to_int() % other.to_int())
        else:
            return Internal(self.to_int() % other)

    def __floordiv__(self, other) -> 'Internal':
        if isinstance(other, Internal):
            return Internal(self.to_int() // other.to_int())
        else:
            return Internal(self.to_int() // other)

    def __rshift__(self, other) -> 'Internal':
        if isinstance(other, Internal):
            return Internal(self.to_int() >> other.to_int())
        else:
            return Internal(self.to_int() >> other)

    def __lshift__(self, other) -> 'Internal':
        if isinstance(other, Internal):
            return Internal(self.to_int() << other.to_int())
        else:
            return Internal(self.to_int() << other)

    def __str__(self) -> str:
        return str(self.to_int())
    
    def __repr__(self) -> str:
        return f"{'-' if self.is_neg else ''}{self.rep}"

    def __getitem__(self, val) -> 'Internal':
        t = Internal(0)
        t.rep = self.rep.__getitem__(val)
        return t

    def __len__(self) -> int:
        return len(self.rep)

    def trim(self) -> None:
        i = len(self.rep)
        while i > 0:
            if self.rep[i - 1] != 0:
                break
            i -= 1
        if i == 0:
            self.rep.clear()
        else:
            self.rep = self.rep[:i]



