#!/usr/bin/env python3
import os
import pandas as pd
import glob
import sys

def combine_results(results_dir, output_file):
    # Find all csv files in the results directory
    all_files = glob.glob(os.path.join(results_dir, "benchmark_breakdown_*.csv"))
    
    if not all_files:
        print(f"No benchmark files found in {results_dir}")
        return

    print(f"Found {len(all_files)} benchmark files.")
    
    combined_df = pd.concat((pd.read_csv(f) for f in all_files), ignore_index=True)
    
    # Trim whitespace from column names and string values
    combined_df.columns = combined_df.columns.str.strip()
    combined_df['OrderBook'] = combined_df['OrderBook'].str.strip()
    combined_df['Scenario'] = combined_df['Scenario'].str.strip()

    # Group by Scenario and OrderBook and calculate mean of numeric columns
    numeric_cols = combined_df.select_dtypes(include=['number']).columns
    averaged_df = combined_df.groupby(['Scenario', 'OrderBook'])[numeric_cols].mean().reset_index()

    # Round logic to keep integers where appropriate
    for col in numeric_cols:
        averaged_df[col] = averaged_df[col].round().astype(int)

    # Sort for readability
    averaged_df.sort_values(by=['Scenario', 'OrderBook'], inplace=True)

    print(f"Writing aggregated results to {output_file}")
    averaged_df.to_csv(output_file, index=False)
    
    # Print a preview
    print("\nPreview of Aggregated Results:")
    print(averaged_df.head().to_string(index=False))

if __name__ == "__main__":
    results_path = os.path.join(os.path.dirname(__file__), "../results")
    output_path = os.path.join(results_path, "combined_results_avg.csv")
    combine_results(results_path, output_path)
