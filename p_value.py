import numpy as np
from scipy import stats

a = np.asarray([3332,3364,3368,3332,3328,3316,3316,3316,3332,3320]) / 100
b = np.asarray([3324,3368,3320,3324,3332,3324,3336,3328,3340,3344]) / 100

t2, p2 = stats.ttest_ind(a,b)
print("t = " + str(t2))
print("p = " + str(p2))

print(np.asarray([0.62, 0.7, 0.42, 0.42, 0.42]).mean())
print(np.asarray([0.98, 0.88, 0.83, 0.41, 0.83]).mean())