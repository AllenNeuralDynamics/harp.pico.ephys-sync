device that accepts Harp timestamps and emits a serial timestamp for ephys data alignment.

## Features
* uses the [harp.core.rp2040](https://github.com/AllenNeuralDynamics/harp.core.rp2040/tree/main) synchronizer only to synchronize against an external Harp synchronization input.
* emits a 1000Bps, little-endian, LSbit-first, 32-bit unsigned integer indicating the current time in seconds.

## Wiring Diagram

*TODO: Wiring Diagram Here*
