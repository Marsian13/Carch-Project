addi x2, x0, 3
addi x3, x0, 8
invmod x4, x2, x3

# addi x2, x0, 10     # a = 10
# addi x3, x0, 17     # m = 17
# invmod x4, x2, x3   # expect x4 = 12

# invmod ans, a, m
# ----------------
# gcd ans, a, m
# check ans, ans, x0
# isprime ans, m, ans
# setmod x0, m, ans
# addi m, m, -2
# binexp ans, a, m
# addi m, m, 2