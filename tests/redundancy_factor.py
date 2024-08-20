import numpy

T = 1
N = 1000
M = N

u = M * (1 - numpy.power((1 - 1/M), N))
uc = T * M * (1 - numpy.power((1 - 1/M), (N/T)))
r = uc / u

print(r)
