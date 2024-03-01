import matplotlib.pyplot as plt
import numpy as np
import random

def draw_graph(results):
    # Prepare the data
    operations = ['Arithmetic', 'Empty Function', 'Sys call']
    machine_names = ['Computer', 'VM', 'Container']
    times = []
    for machine in machine_names:
        machine_times = [results[machine]['arithmetic'], results[machine]['empty'], results[machine]['Sys call']]
        times.append(machine_times)

    # Set the width of the bars
    bar_width = 0.2

    # Set the positions of the bars on the x-axis
    r1 = np.arange(len(times[0]))
    r2 = [x + bar_width for x in r1]
    r3 = [x + bar_width for x in r2]

    # Create a bar chart
    plt.bar(r1, times[0], width=bar_width, label=machine_names[0])
    plt.bar(r2, times[1], width=bar_width, label=machine_names[1])
    plt.bar(r3, times[2], width=bar_width, label=machine_names[2])

    # Set the axes labels and title
    plt.xlabel('Operation Type')
    plt.ylabel('Time (seconds)')
    plt.title('Running Times (log scale)')
    plt.yscale('log')

    # Set the x-axis tick labels
    plt.xticks([r + bar_width for r in range(len(times[0]))], operations)

    # Show the legend
    plt.legend()

    # Show the graph

    plt.show()

if __name__ == '__main__':

    # Define the random results
    random_results = {
        'Computer': {
            'arithmetic':2.968000,
            'empty': 2.119000,
            'Sys call': 482.678000
        },
        'VM': {
            'arithmetic': 3.310000,
            'empty': 12.458000,
            'Sys call': 697.459000
        },
        'Container': {
            'arithmetic': 2.325000,
            'empty': 2.100000,
            'Sys call': 523.862000
        }
    }

    # Call the draw_graph function with the random results
    draw_graph(random_results)
