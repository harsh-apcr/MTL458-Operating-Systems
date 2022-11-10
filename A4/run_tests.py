import subprocess
import numpy as np

filename = "./main"
outf = open("outfile", 'w')
for i in range(10, 51): 
    input = ''
    for j in range(i):
        x = np.random.randint(0, 4)
        if x == 0:
            input += 'N'
        if x == 1:
            input += 'W'
        if x == 2:
            input += 'S'
        if x == 3:
            input += 'E'
    print(f'main on input of length {i} started')
    subprocess.call([filename, input], stdout=outf)
    print(f'main on input of length {i} completed')
    subprocess.run(['./check', 'outfile'])
    