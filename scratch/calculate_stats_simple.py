import pandas as pd
import numpy as np

# Load results
df = pd.read_csv('results/results.csv')
df = df[df['mode'] == 'direct']

scenarios = ['mixed', 'dense_full', 'tight_spread']
implementations = ['array', 'map', 'pool', 'vector', 'hybrid']

def welch_t_stat(x1, x2):
    n1, n2 = len(x1), len(x2)
    m1, m2 = np.mean(x1), np.mean(x2)
    s1, s2 = np.var(x1, ddof=1), np.var(x2, ddof=1)
    
    t = (m1 - m2) / np.sqrt(s1/n1 + s2/n2)
    
    # Degrees of freedom (Welch-Satterthwaite)
    v1 = s1/n1
    v2 = s2/n2
    df_num = (v1 + v2)**2
    df_den = (v1**2 / (n1-1)) + (v2**2 / (n2-1))
    df_final = df_num / df_den
    
    return t, df_final

for s in scenarios:
    print(f"\nScenario: {s}")
    array_data = df[(df['scenario'] == s) & (df['book'] == 'array')]['latency_ns'].values
    for impl in ['map', 'pool', 'vector', 'hybrid']:
        other_data = df[(df['scenario'] == s) & (df['book'] == impl)]['latency_ns'].values
        t, dgf = welch_t_stat(array_data, other_data)
        print(f"Array vs {impl: <6}: t={t:.2f}, df={dgf:.2f}")
