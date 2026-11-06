# Agent Model Providers

## Purpose
Defines the pluggable interface through which the layout agent reaches an AI model, and the confidentiality rules for layout data. Layouts and PDK data are often confidential, and may be under foundry NDAs. So no data leaves the device unless the user explicitly enables a provider for that project.

## Requirements

### Requirement: Provider interface
Hefesto SHALL define a provider interface that supports chat with tool calling, streaming responses, cancellation, and reporting of token or cost usage. Hefesto SHALL ship these implementations:
- a cloud API provider, for example the Anthropic Claude API
- an OpenAI-compatible HTTP provider, for self-hosted or on-prem gateways
- an offline provider for local models where the platform supports them (task coverage on iPadOS: see Q-AGENT-3)

The model, endpoint and parameters SHALL be configuration. No model identifier SHALL be hard-coded.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Switch to on-prem gateway
- **WHEN** a project's provider is changed to an OpenAI-compatible endpoint at an internal URL
- **THEN** subsequent agent tasks SHALL use that endpoint, and no requests SHALL go to the cloud provider

### Requirement: Disabled by default, per-project opt-in
For every project, the agent provider SHALL default to "disabled". Enabling a provider SHALL require an explicit user action, and that action SHALL show which data categories will be sent. The provider choice SHALL be stored in the project settings (capability `hefesto-settings`), so a team can pin, for example, "on-prem only" for a confidential project. Restricting providers for confidential PDKs is open (see Q-AGENT-5), and so are the App Store disclosure rules for the agent (see Q-AGENT-4).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Fresh project
- **WHEN** a user opens the agent panel in a new project
- **THEN** Hefesto SHALL ask the user to choose a provider, and SHALL NOT send any data before the choice is made

### Requirement: Data minimization and audit
Each request SHALL include only the context that the task needs: the cells in scope, the relevant rules and the netlist. A per-project redaction option SHALL replace cell, net and label names with stable placeholders before sending, and SHALL map them back in the responses. Every request and response SHALL be recorded in a local audit log that the user can inspect and export.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Redacted names
- **GIVEN** redaction is enabled
- **WHEN** a task about net `VDD_core` is sent
- **THEN** the request SHALL contain a placeholder such as `N17` instead of `VDD_core`, and the applied commands SHALL use the real name

### Requirement: Credential storage
API keys and tokens SHALL be stored in the platform credential store: the Keychain on Apple platforms, Secret Service on Linux. They SHALL NOT be stored in project files, settings exports or logs.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Export settings
- **WHEN** the user exports their preferences
- **THEN** the exported file SHALL NOT contain any provider credential
