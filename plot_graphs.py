import subprocess
import re
import pandas as pd
import matplotlib.pyplot as plt
import os

# Configuration
EXECUTABLE = "./cache_simulator.exe"  # Path to the simulator executable
TRACE_PREFIX = "app1"       # Trace file prefix (e.g., app_proc0.trace)
RESULTS_FILE = "cache_sim_results.csv"
PLOT_DIR = "plots"         # Directory to save plots

# Parameter values to vary (3 values each, including default)
S_VALUES = [ 4 , 6 , 8]       # Set index bits
E_VALUES = [2, 4, 6]       # Associativity
B_VALUES = [4, 5, 6]       # Block bits

# Default parameters
DEFAULT_S = 6
DEFAULT_E = 2
DEFAULT_B = 5

# Ensure plot directory exists
if not os.path.exists(PLOT_DIR):
    os.makedirs(PLOT_DIR)

# Function to run the simulator and extract max execution time
def run_simulation(param, value, s, E, b):
    cache_size = (1 << s) * E * (1 << b)  # Cache size in bytes
    cmd = [EXECUTABLE, "-t", TRACE_PREFIX, "-s", str(s), "-E", str(E), "-b", str(b)]
    
    print(f"Running simulation with {param}={value} (s={s}, E={E}, b={b}, CacheSize={cache_size} bytes)...")
    
    try:
        # Run the simulator and capture output
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        output = result.stdout
        
        # Extract max execution time (last occurrence of the pattern)
        match = re.search(r"Maximum Execution Time \(cycles\): (\d+)", output)
        if not match:
            print(f"Error: Could not extract MaxExecutionTime for {param}={value}.")
            return None
        max_time = int(match.group(1))
        
        return {"Parameter": param, "Value": value, "MaxExecutionTime": max_time, "CacheSize": cache_size}
    
    except subprocess.CalledProcessError as e:
        print(f"Error: Simulation failed for {param}={value}. Error: {e}")
        return None
    except Exception as e:
        print(f"Error: Unexpected error for {param}={value}. Error: {e}")
        return None

# Check if trace files exist
for i in range(4):
    trace_file = f"{TRACE_PREFIX}_proc{i}.trace"
    if not os.path.exists(trace_file):
        print(f"Error: Trace file {trace_file} not found.")
        exit(1)

# Check if executable exists
if not os.path.exists(EXECUTABLE):
    print(f"Error: Simulator executable {EXECUTABLE} not found.")
    exit(1)

# Collect results
results = []

# Vary set index bits (s)
for s in S_VALUES:
    result = run_simulation("SetIndexBits", s, s, DEFAULT_E, DEFAULT_B)
    if result:
        results.append(result)

# Vary associativity (E)
for E in E_VALUES:
    result = run_simulation("Associativity", E, DEFAULT_S, E, DEFAULT_B)
    if result:
        results.append(result)

# Vary block bits (b)
for b in B_VALUES:
    result = run_simulation("BlockBits", b, DEFAULT_S, DEFAULT_E, b)
    if result:
        results.append(result)

# Save results to CSV
if results:
    df = pd.DataFrame(results)
    df.to_csv(RESULTS_FILE, index=False)
    print(f"Results saved to {RESULTS_FILE}")
else:
    print("No results to save. Exiting.")
    exit(1)

# Plotting
# Split data by parameter
set_index_data = df[df['Parameter'] == 'SetIndexBits']
associativity_data = df[df['Parameter'] == 'Associativity']
block_bits_data = df[df['Parameter'] == 'BlockBits']

# Plot 1: Maximum Execution Time vs. Set Index Bits
plt.figure(figsize=(8, 6))
plt.plot(set_index_data['Value'], set_index_data['MaxExecutionTime'], marker='o', label='Set Index Bits')
plt.xlabel('Set Index Bits (s)')
plt.ylabel('Maximum Execution Time (cycles)')
plt.title('Maximum Execution Time vs. Set Index Bits')
plt.grid(True)
plt.legend()
plt.savefig(os.path.join(PLOT_DIR, 'set_index_bits.png'))
plt.close()

# Plot 2: Maximum Execution Time vs. Associativity
plt.figure(figsize=(8, 6))
plt.plot(associativity_data['Value'], associativity_data['MaxExecutionTime'], marker='s', label='Associativity')
plt.xlabel('Associativity (E)')
plt.ylabel('Maximum Execution Time (cycles)')
plt.title('Maximum Execution Time vs. Associativity')
plt.grid(True)
plt.legend()
plt.savefig(os.path.join(PLOT_DIR, 'associativity.png'))
plt.close()

# Plot 3: Maximum Execution Time vs. Block Bits
plt.figure(figsize=(8, 6))
plt.plot(block_bits_data['Value'], block_bits_data['MaxExecutionTime'], marker='^', label='Block Bits')
plt.xlabel('Block Bits (b)')
plt.ylabel('Maximum Execution Time (cycles)')
plt.title('Maximum Execution Time vs. Block Bits')
plt.grid(True)
plt.legend()
plt.savefig(os.path.join(PLOT_DIR, 'block_bits.png'))
plt.close()

# Plot 4: Maximum Execution Time vs. Cache Size
plt.figure(figsize=(8, 6))
plt.plot(set_index_data['CacheSize'], set_index_data['MaxExecutionTime'], marker='o', label='Varying s')
plt.plot(associativity_data['CacheSize'], associativity_data['MaxExecutionTime'], marker='s', label='Varying E')
plt.plot(block_bits_data['CacheSize'], block_bits_data['MaxExecutionTime'], marker='^', label='Varying b')
plt.xlabel('Cache Size (bytes)')
plt.ylabel('Maximum Execution Time (cycles)')
plt.title('Maximum Execution Time vs. Cache Size')
plt.legend()
plt.grid(True)
plt.savefig(os.path.join(PLOT_DIR, 'cache_size.png'))
plt.close()

print(f"Plots saved in {PLOT_DIR}/: set_index_bits.png, associativity.png, block_bits.png, cache_size.png")