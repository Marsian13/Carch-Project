li x2, 8
li x3, 13
invmod x4, x2, x3

# li x2, 10     # a = 10
# li x3, 17     # m = 17
# invmod x4, x2, x3   # expected x4 = 12

# invmod ans, a, m
# ----------------
# gcd ans, a, m
# check ans, ans, x0
# isprime ans, m, ans
# setmod x0, m, ans
# addi m, m, -2
# binexp ans, a, m
# addi m, m, 2