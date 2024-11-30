def test(A, base):
    res = []
    n = int(A)
    while n:
        res.append(n % base)
        n //= base
    return res[::-1]  # Return reversed to keep most significant digits first

def to_base(n, to_base, from_base):
    # Initialize result array (size based on input)
    res = [0] * (len(n) * 10)  # More dynamic size allocation
    size = 0
    
    # Process digits from most significant to least significant
    for x in n:
        carry = x
        j = 0
        while j <= size or carry:
            # Get current digit value if it exists
            current = res[j] if j < len(res) else 0
            # Multiply current digit by from_base and add carry
            temp = current * from_base + carry
            # Store digit in target base
            res[j] = temp % to_base
            # Calculate new carry
            carry = temp // to_base
            j += 1
        size = max(j, size)
    
    # Trim leading zeros and reverse
    while size > 0 and res[size-1] == 0:
        size -= 1
    
    # Convert to hex string
    hex_map = "0123456789abcdef"
    return ''.join(hex_map[x] for x in reversed(res[:size]))

# Test the conversion
A = "827394650391827364598273645982736459827364598273645982736459827364598273645982736459982"
base = 2 ** 31
res = test(A, base)
print("Number in base 2^31:", res)
result = to_base(res, 16, base)
print("Number in base 16:", result)

# Verify the result by converting back to decimal
def verify_hex(hex_str):
    return hex(int(A))[2:] == hex_str

print("Verification (matches built-in hex):", verify_hex(result))
