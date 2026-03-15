import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import sys
import os

def plot_results(csv_file):
    if not os.path.exists(csv_file):
        print(f"Error: {csv_file} not found.")
        return

    df = pd.read_csv(csv_file)
    
    # Create plots directory if it doesn't exist
    os.makedirs('plots', exist_ok=True)

    # 1. Direct Mode Comparison (Algorithmic Latency)
    direct_df = df[df['Mode'] == 'direct']
    if not direct_df.empty:
        plt.figure(figsize=(10, 6))
        sns.barplot(data=direct_df, x='Book', y='Latency_ns', hue='Book', palette='viridis', legend=False)
        plt.title('Algorithmic Latency Comparison (Direct Mode)')
        plt.ylabel('Latency (ns)')
        plt.savefig('plots/latency_direct.png')
        print("Generated plots/latency_direct.png")

    # 2. Systemic Impact (Direct vs Gateway)
    # The CSV now has a unified 'Latency_ns' which is Algo for Direct and E2E for Gateway
    plt.figure(figsize=(12, 7))
    sns.barplot(data=df, x='Book', y='Latency_ns', hue='Mode')
    plt.title('Systemic Performance Gap (Algo vs Full System)')
    plt.ylabel('Mean Latency (ns)')
    plt.yscale('log')
    plt.savefig('plots/systemic_comparison.png')
    print("Generated plots/systemic_comparison.png")

    # 3. Throughput (Gateway Only)
    gateway_df = df[df['Mode'] == 'gateway']
    if not gateway_df.empty:
        plt.figure(figsize=(10, 6))
        sns.barplot(data=gateway_df, x='Book', y='Throughput', hue='Book', palette='magma', legend=False)
        plt.title('System Throughput (Orders/sec)')
        plt.ylabel('Throughput (ops/sec)')
        plt.savefig('plots/throughput_gateway.png')
        print("Generated plots/throughput_gateway.png")

    # 4. End-to-End Latency Detail
    if 'Latency_ns' in df.columns:
        gateway_only = df[df['Mode'] == 'gateway']
        if not gateway_only.empty:
            plt.figure(figsize=(10, 6))
            sns.barplot(data=gateway_only, x='Book', y='Latency_ns', hue='Book', palette='magma', legend=False)
            plt.title('Total End-to-End Latency (Gateway Mode)')
            plt.ylabel('E2E Latency (ns)')
            plt.yscale('log')
            plt.savefig('plots/wire_to_match_server.png')
            print("Generated plots/wire_to_match_server.png")

if __name__ == "__main__":
    file_path = sys.argv[1] if len(sys.argv) > 1 else 'results/results.csv'
    plot_results(file_path)
