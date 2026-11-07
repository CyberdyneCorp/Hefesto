# Open questions: AGENT

### Q-AGENT-1: Routing engine for schematic-driven layout
- Kind: ambiguity
- Magic behavior: Magic's routers are a legacy channel router and a maze router, designed for digital nets and simple metal stacks.
- Evidence: capabilities router-channel, router-maze, router-maze-iroute. The agent's routing tools wrap only `router-maze`, `router-maze-iroute` and `wiring-tool` (capability `agent-schematic-driven-layout`), because the channel router is a legacy, desktop-only tool (capability `hefesto-ipad-feature-scope`, roadmap M12) and the agent must also work on iPadOS.
- Options:
  - extend the maze router with analog constraints (width per net, shielding, symmetric routing)
  - specify a new detailed router capability
  - let the agent draw routes with paint commands, checked by DRC
- Recommendation: start with a constraint-aware maze router plus agent-driven paint, and spec a dedicated analog router if the benchmark shows it is needed.

### Q-AGENT-2: Representation of PDK rules in the agent context
- Kind: ambiguity
- Options:
  - compile DRC styles into a compact rule table (width, spacing, enclosure, area per layer)
  - add PDK-specific design guidance documents as retrieval context
- Recommendation: compiled rule table always; guidance documents optional, per PDK.

### Q-AGENT-3: On-device models on iPad
- Kind: platform
- Question: Is an offline model on iPadOS capable enough for any agent task? Memory and Neural Engine constraints apply.
- Recommendation: support the offline provider for natural-language commands and explanations only, until the evaluation suite shows it is adequate for generation tasks.

### Q-AGENT-4: App Store and AI-feature policy
- Kind: platform
- Question: Do the App Store rules on generative AI features, user consent, data disclosure and the privacy nutrition label apply to the agent?
- Recommendation: legal review before the first iPadOS release with the agent.

### Q-AGENT-5: Export control and foundry NDAs
- Kind: ambiguity
- Question: Some PDKs and designs are NDA-restricted or export-controlled. Should Hefesto block cloud providers when a PDK is marked confidential?
- Recommendation: add a per-PDK flag `confidential` that restricts the agent to on-prem or offline providers.

### Q-AGENT-6: Default limits
- Kind: ambiguity
- Question: What are the default cost, time and iteration limits per task type?
- Recommendation: calibrate them from the benchmark suite.
