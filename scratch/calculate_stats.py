import pandas as pd
from scipy import stats

# Load results
df = pd.read_csv('results/results.csv')

# Only look at direct mode
df = df[df['mode'] == 'direct']

scenarios = ['mixed', 'dense_full', 'tight_spread']
implementations = ['array', 'map', 'pool', 'vector', 'hybrid']

def run_stats(scenario):
    print(f"\n--- Scenario: {scenario} ---")
    data = {}
    for impl in implementations:
        data[impl] = df[(df['scenario'] == scenario) & (df['book'] == impl)]['latency_ns'].values
    
    # Array is the baseline we are testing against
    array_data = data['array']
    
    # We compare Array against 4 other implementations
    # Bonferroni alpha = 0.05 / 4 = 0.0125
    alpha = 0.05
    n_comparisons = 4
    alpha_adj = alpha / n_comparisons
    
    for impl in ['map', 'pool', 'vector', 'hybrid']:
        other_data = data[impl]
        # Welch's T-test (equal_var=False)
        t_stat, p_val = stats.ttest_ind(array_data, other_data, equal_var=False)
        
        is_significant = p_val < alpha_adj
        print(f"Array vs {impl: <6}: p={p_val:.2e} | Significant: {is_significant}")

for s in scenarios:
    run_stats(s)
