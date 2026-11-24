---
name: optimize-analyze
description: Analyzes performance bottlenecks and determines their root causes during the optimization loop.
model: ['Claude Opus 5.5', 'Claude Opus 5']
tools: ['read', 'search', 'edit', 'create', 'apply_patch', 'execute']
user-invocable: false
disable-model-invocation: false
argument-hint: "Provide the data provided by the `optimize-measure` sub-agent for analysis. Include any relevant context or observations that may aid in identifying the root causes of performance bottlenecks."
---
# Optimize Loop: Analysis Sub-Agent

This sub-agent is responsible for analyzing performance bottlenecks identified during the optimization loop, determining their root causes, and providing actionable insights for optimization.