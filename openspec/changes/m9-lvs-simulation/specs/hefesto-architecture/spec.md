## ADDED Requirements

### Requirement: Long-lived worker processes
The platform services SHALL offer a worker service, separate from running a process to completion. The service SHALL:
- start a worker executable;
- return a channel that sends and receives length-prefixed binary frames over the worker's standard input and output;
- report every received frame and the worker's exit to the core through callbacks;
- let the core terminate the worker.

The core SHALL declare only the interface. The implementations live in the hosts (the desktop shells and the headless tool). On platforms without process creation, the service SHALL report itself unsupported, and the features that need it SHALL report their unavailability without affecting any other feature.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7, docs/decisions/cross-cutting.md#X-4) (See Q-SIM-21.)

#### Scenario: Worker exits
- **GIVEN** a worker started through the service on Linux
- **WHEN** the worker exits with status 3
- **THEN** the core SHALL receive the exit callback with status 3, and later sends on that channel SHALL fail with a diagnostic instead of blocking

#### Scenario: No process creation
- **GIVEN** the iPadOS host
- **WHEN** the core asks whether the worker service is supported
- **THEN** the answer SHALL be no, and starting a worker SHALL fail with the unsupported-platform diagnostic
