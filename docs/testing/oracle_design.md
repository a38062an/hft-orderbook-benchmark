# Test Oracle Specification for the Order Book

## Purpose

This document defines how correctness is decided for order-book behavior.

- Coverage demonstrates execution.
- Oracles demonstrate semantic correctness.

An oracle is the acceptance rule used to classify observed output as correct or incorrect for a defined input sequence.

## Terminology

### Oracle

The decision rule that determines whether the observed result is correct for a test input.

### Contract

The expected behavior promised by an API operation, usually expressed as preconditions and postconditions.

### Invariant

A property that must remain true for every valid system state, regardless of operation sequence.

## Scope

The oracle specification applies to all implementations created through the common interface and factory:

- map
- vector
- array
- hybrid
- pool

Primary contract tests are implemented in [tests/unit/orderbook_contract_test.cpp](tests/unit/orderbook_contract_test.cpp).

## Oracle Model

Correctness is evaluated by combining four oracle classes.

## 1. State oracles

Postconditions on persistent book state after operations.

Core predicates:

- order count is correct
- best bid is correct
- best ask is correct
- expected level removal or level persistence occurs

## 2. Value oracles

Exact equality on produced trade fields.

Core predicates:

- trade quantity is correct
- trade price is correct
- trade buy id and sell id are correct

## 3. Sequence oracles

Ordering constraints on trade outcomes.

Core predicates:

- FIFO is preserved at a single price level
- one-to-many matching produces expected trade order

## 4. Non-mutation oracles

Invalid inputs or unknown operations must not corrupt valid state.

Core predicates:

- unknown cancel and modify are no-op
- repeated cancel of removed id is no-op

## Formalized Oracle Properties

O-01 Best-price monotonicity under non-improving insertions:

- adding lower bid must not raise best bid
- adding higher ask must not lower best ask

O-02 Best-price fallback under top-level removal:

- canceling current best bid reveals next best bid or empty sentinel
- canceling current best ask reveals next best ask or empty sentinel

O-03 Partial-fill conservation:

- if one side has larger quantity, exactly one residual order remains
- matched quantity equals minimum of aggressor/resting pair

O-04 FIFO correctness at equal price:

- earlier order id at same price must fill before later order id

O-05 One-to-many matching consistency:

- cumulative matched quantity equals original crossable quantity
- trade sequence aligns with price-time priority constraints

O-06 Unknown-operation safety:

- unknown ids must not change count, best bid, or best ask

## Evidence Mapping

| Oracle property | Representative tests |
| --- | --- |
| O-01 | EdgeCase_AddLowerBidDoesNotChangeBestBid, EdgeCase_AddHigherAskDoesNotChangeBestAsk |
| O-02 | EdgeCase_CancelBestBidFallsBackToNextLevel, EdgeCase_CancelBestAskFallsBackToNextLevel |
| O-03 | EdgeCase_PartialFillSellSideSurvives, EdgeCase_PartialFillBuySideSurvives |
| O-04 | EdgeCase_FifoAtSinglePrice |
| O-05 | EdgeCase_OneBidMatchesMultipleAsks, EdgeCase_OneAskMatchesMultipleBids |
| O-06 | ErrorCase_UnknownOperationsAreNoop, ErrorCase_UnknownOperationsDoNotMutateExistingBook, ErrorCase_CancelAlreadyCancelledOrderIsNoop |

All listed tests are in [tests/unit/orderbook_contract_test.cpp](tests/unit/orderbook_contract_test.cpp).

## Negative-Example Sanity (Would-Fail Reasoning)

The oracle suite is designed so plausible defects fail deterministically.

Examples:

1. Wrong trade price assignment:
   - value-oracle checks on trade price fail in match and partial-fill tests.
2. Broken FIFO behavior at same price:
   - sequence-oracle checks fail in FIFO test.
3. Unknown operation mutates state:
   - non-mutation oracle tests fail on count or best-price assertions.

## Acceptance Criteria

An implementation is accepted only if all are true:

1. Every contract test for all five implementations passes.
2. All oracle properties O-01 through O-06 are satisfied.
3. No-op and repeated invalid operations leave valid state unchanged.

## Duplicate-ID Policy

Duplicate order-id insertion is rejected as a no-op. Existing book state must remain unchanged.

Rationale:

- Aligns with standard CLOB safety and deterministic replay behavior.
- Avoids silent replacement side effects and priority ambiguity.
- Keeps cancel/modify semantics stable because one live id maps to one live order.

## Practical Test Authoring Rule

Every new test should include:

1. precondition assertions,
2. one explicit action block,
3. postcondition assertions mapped to at least one oracle property.

This keeps coverage and correctness evidence aligned.

## Selected References

The methodology used here is aligned with established software-testing literature.

1. Meyer, B. (1997). *Object-Oriented Software Construction* (2nd ed.). Prentice Hall.
   - Canonical source for Design by Contract reasoning.

2. Claessen, K., & Hughes, J. (2000). QuickCheck: A lightweight tool for random testing of Haskell programs. In *Proceedings of ICFP 2000*. ACM.
   - DOI: `10.1145/351240.351266`
   - Foundation for property/invariant-driven randomized testing.

3. Ammann, P., & Offutt, J. (2016). *Introduction to Software Testing* (2nd ed.). Cambridge University Press.
   - Standard reference for test design, oracles, and adequacy criteria.

4. Chen, T. Y., Cheung, S. C., & Yiu, S. M. (1998). Metamorphic testing: A new approach for generating next test cases.
   - Technical report reference commonly cited as the basis of metamorphic testing.

5. Chen, T. Y., Kuo, F.-C., Liu, H., Poon, P.-L., Towey, D., Tse, T. H., & Zhou, Z. Q. (2018). Metamorphic testing: A review of challenges and opportunities. *ACM Computing Surveys*, 51(1), Article 4.
   - DOI: `10.1145/3143561`

6. McKeeman, W. M. (1998). Differential testing for software. *Digital Technical Journal*, 10(1), 100-107.
   - Practical basis for reference-model (differential) oracle strategies.
