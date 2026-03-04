import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import glob
import os
import sys

# Find the latest breakdown CSV file
list_of_files = glob.glob('../results/benchmark_breakdown_*.csv')
if not list_of_files:
    print("No breakdown CSV files found.")
    sys.exit(1)
latest_file = max(list_of_files, key=os.path.getctime)
print(f"Plotting data from: {latest_file}")

# Load the data
try:
    df = pd.read_csv(latest_file)
except Exception as e:
    print(f"Error reading {latest_file}: {e}")
    sys.exit(1)

# Ensure output directory exists
os.makedirs('../docs/plots', exist_ok=True)

# 1. Total Latency Comparison (Mean vs P99)
plt.figure(figsize=(14, 8))
sns.set_theme(style="whitegrid")
ax = sns.barplot(
    data=df,
    x='Scenario',
    y='Total_Mean',
    hue='OrderBook',
    palette='viridis'
)
plt.title('Mean Total Latency by Scenario and OrderBook (Lower is Better)')
plt.ylabel('Mean Latency (System Ticks)')
plt.xticks(rotation=45)
plt.tight_layout()
plt.savefig('../docs/plots/mean_total_latency.png', dpi=300)
print("Generated mean_total_latency.png")

# 2. P99 Latency Comparison
plt.figure(figsize=(14, 8))
ax = sns.barplot(
    data=df,
    x='Scenario',
    y='Total_P99',
    hue='OrderBook',
    palette='magma'
)
plt.title('P99 Total Latency by Scenario and OrderBook (Lower is Better)')
plt.ylabel('P99 Latency (System Ticks)')
plt.xticks(rotation=45)
plt.tight_layout()
plt.savefig('../docs/plots/p99_total_latency.png', dpi=300)
print("Generated p99_total_latency.png")

# 3. Breakdown of Operations for a specific scenario (e.g., ReadHeavy_100K)
read_heavy_df = df[df['Scenario'] == 'ReadHeavy_100K']
if not read_heavy_df.empty:
    melted_df = read_heavy_df.melt(
        id_vars='OrderBook', 
        value_vars=['Insert_Mean', 'Lookup_Mean', 'Match_Mean'],
        var_name='Operation', 
        value_name='Latency'
    )
    plt.figure(figsize=(10, 6))
    sns.barplot(
        data=melted_df,
        x='OrderBook',
        y='Latency',
        hue='Operation',
        palette='Set2'
    )
    plt.title('Latency Breakdown for ReadHeavy_100K Scenario')
    plt.ylabel('Mean Latency (System Ticks)')
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.savefig('../docs/plots/read_heavy_breakdown.png', dpi=300)
    print("Generated read_heavy_breakdown.png")
    
print("All plots generated successfully in ../docs/plots/")

