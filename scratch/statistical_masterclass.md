# Statistical Masterclass: HFT Performance Analysis

This document provides a rigorous mathematical explanation of the statistical framework used in your dissertation. It explains how we move from raw latency measurements to definitive scientific proof.

## 1. The Core Formula: Welch's T-Test
We use the **Welch's t-test** because it does not assume equal variances (jitter) between implementations.

The formula for the $t$-statistic is:
$$t = \frac{\bar{x}_1 - \bar{x}_2}{\sqrt{\frac{s_1^2}{n_1} + \frac{s_2^2}{n_2}}}$$

Where:
- $\bar{x}_1, \bar{x}_2$: The mean latencies of the two groups.
- $s_1, s_2$: The sample standard deviations (jitter).
- $n_1, n_2$: The number of observations (replicates).

### Why is $t$ negative?
In your report, you see values like $t = -75.09$.
- If you compare **Group 1 (Array: 200ns)** against **Group 2 (Map: 600ns)**.
- The numerator is $(200 - 600) = -400$.
- A **negative** $t$ simply indicates that the first implementation is **faster** (lower latency) than the second. It is the mathematical signature of a performance "Winner."

---

## 2. Degrees of Freedom ($df$): The Welch Adjustment
Standard t-tests use $n-1$. However, because our variances ($s_1, s_2$) are different, we must use the **Welch-Satterthwaite equation** to calculate the "effective" degrees of freedom:

$$\nu \approx \frac{\left(\frac{s_1^2}{n_1} + \frac{s_2^2}{n_2}\right)^2}{\frac{s_1^4}{n_1^2\nu_1} + \frac{s_2^4}{n_2^2\nu_2}}$$

Where $\nu_1 = n_1 - 1$ and $\nu_2 = n_2 - 1$.
- This is why you see decimals like **$df = 43.87$**. It indicates that the statistical model has been fine-tuned to the specific jitter of each order book.

---

## 3. From $t$ to $p$: The Probability of Fluke
The $p$-value is the area under the **Student's t-distribution** curve.
- Think of the t-distribution as a bell curve centered at $0$.
- If $t = 0$, the two books are identical (no difference).
- If $t = -75$, your result is **75 standard errors** away from the center.
- The area under the curve that far out is effectively zero. This area is the $p$-value ($p < 0.0001$).
- It tells you that the chance of this performance gap being "just luck" is less than 0.01%.

---

## 4. Bonferroni Correction: The Multi-Test Guard
When you perform $m$ comparisons (e.g. Array vs Map, Array vs Pool, etc.), the risk of a "False Positive" grows. We correct for this by adjusting our significance threshold ($\alpha$):

$$\alpha_{adj} = \frac{\alpha}{m}$$

- **Input**: Original target $\alpha = 0.05$ (5% error tolerance) and $m = 4$ comparisons.
- **Output**: Adjusted target $\alpha_{adj} = 0.0125$.
- **Interpretation**: A result is ONLY "Significant" if its $p$-value is less than $0.0125$. This ensures the highest level of academic integrity.

---

## 5. Confidence Intervals: The Range of Truth
The 95% Confidence Interval (CI) is calculated as:
$$CI = \bar{x} \pm t^* \cdot \frac{s}{\sqrt{n}}$$
Where $t^*$ is the critical value from the t-distribution.

- **The Overlap Rule**: If the 95% CI of the Array and the Map **do not overlap**, the difference is statistically significant.
- **In H3 (Masking)**: In Gateway mode, the jitter is so high that the $\pm$ range becomes huge, causing the intervals to swallow each other. This is the definitive proof of masking.
