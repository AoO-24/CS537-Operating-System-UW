import matplotlib.pyplot as plt
from collections import defaultdict

# Initialize a dictionary to store data for each PID
data = defaultdict(list)

# Read data from the text file
with open('stat2.txt', 'r') as file:
    lines = file.readlines()
    for line in lines:
        parts = line.strip().split(', ')
        pid = int(parts[0].split(': ')[1])
        exec_ticks = int(parts[1].split(': ')[1])
        current_tickets = int(parts[2].split(': ')[1])
        
        data[pid].append((current_tickets, exec_ticks))

# Plot the data
plt.figure(figsize=(10, 6))

for pid, points in data.items():
    x, y = zip(*sorted(points))  # sort by current_tickets
    plt.plot(x, y, marker='o', label=f'PID {pid}')

plt.xlabel('Time (ticks)')
plt.ylabel('Execution ticks / process')
plt.title('xv6 P4 RR Evaluation')
plt.legend() 

# Save the plot as a PNG file
plt.savefig('plot6.png')

# Show the plot (optional)
plt.show()