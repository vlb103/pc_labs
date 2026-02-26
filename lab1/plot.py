import csv
import matplotlib.pyplot as plt

threads = []
times = []

with open('1.csv', 'r') as file:
    next(file) 
    for row in csv.reader(file):
        threads.append(str(row[0]))
        times.append(int(row[1]))

plt.plot(threads, times, marker='o')

plt.xticks(threads)

plt.xlabel('threads')
plt.ylabel('time (ms)')

plt.grid(True)
plt.show()