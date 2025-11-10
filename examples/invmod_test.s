addi x2, x0, 3
addi x3, x0, 7
invmod x4, x2, x3

# addi x2, x0, 10     # a = 10
# addi x3, x0, 17     # m = 17
# invmod x4, x2, x3   # expect x4 = 12

# addi x2, x0, 10 
# addi x3, x0, 17
# setmod x0, x3, x0
# addi x3, x3, -2
# binexp x4, x2, x3