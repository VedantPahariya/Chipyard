# Plan: Multicore CVA6 + Ara + Gemmini SoC Configuration

**TL;DR:** Creating a SoC with multicore CVA6, Ara vector unit, and Gemmini accelerator is **not directly feasible** with Chipyard's current infrastructure. CVA6 lacks RoCC (Rocket Custom Coprocessor) interface support required by Gemmini, and Ara only integrates with Rocket/Shuttle cores. The research reveals three critical blockers: (1) `CVA6Tile` doesn't extend `HasLazyRoCC` trait, (2) Ara's attachment logic only recognizes `RocketTileAttachParams` and `ShuttleTileAttachParams`, not `CVA6TileAttachParams`, and (3) CVA6's non-coherent AXI interface is officially discouraged for multicore use. **Recommended approach:** Use **Rocket cores** instead of **CVA6** for immediate functionality.

## Steps

1.  **Understand architectural incompatibilities** — Review how `CVA6Tile.scala` doesn't extend `HasLazyRoCC` (line 110+) unlike `RocketTile.scala` (line 65), preventing Gemmini attachment. Examine `AraConfigs.scala` showing Ara only pattern-matches Rocket/Shuttle tile types (lines 11-65), not CVA6.
2.  **Implement proven alternative with Rocket cores** — Create configuration in new file `MultiRocketAraGemminiConfig.scala` using `WithNBigCores(4)` for 4 Rocket cores, stack with `WithAraRocketVectorUnit(4096, 4)` for 4-lane Ara (targets all cores), add `WithMultiRoCC` + `WithMultiRoCCFromBuildRoCC(0,1,2,3)` to attach Gemmini per-hart, include `DefaultGemminiConfig` and `WithSystemBusWidth(128)`. Reference `ReRoCCManyGemminiConfig` (lines 41-48) for multi-accelerator pattern.
3.  **Test incrementally with simpler configurations** — Build intermediate configs: (a) single Rocket + Gemmini, (b) single Rocket + Ara, (c) dual Rocket + Gemmini, then (d) full multicore + both accelerators. Use Chipyard's `make CONFIG=<YourConfig>` build flow in `sims/verilator/` to validate each step.
4.  **Verify system bus width and memory hierarchy** — Ensure 128-bit system bus matches Gemmini's DMA requirements and Ara's 4-lane memory bandwidth needs. Check TileLink crossbar configuration handles multiple master nodes (4 cores + 4 Gemmini + Ara memory ports). Reference `DualLargeBoomAndSingleRocketConfig` for heterogeneous SoC patterns.

## Further Considerations

1.  **CVA6 requirement flexibility?** If CVA6 is mandatory, expect 3-4 months implementation: adding `HasLazyRoCC` to `CVA6Tile`, creating CV-X-IF to Ara bridge, implementing AXI-TileLink coherency adapter. Consider heterogeneous approach with CVA6 for control + Rocket for compute cores instead?
2.  **Per-core accelerator attachment strategy?** MultiRoCC allows selective Gemmini attachment (e.g., only cores 0-1 get Gemmini, all get Ara). Clarify if all cores need identical accelerators or heterogeneous capabilities preferred?
3.  **Cache coherency and memory consistency?** Rocket provides coherent TileLink infrastructure proven for multicore. Validate shared memory model between Ara vector loads/stores and Gemmini DMA accesses. Consider separate scratchpad regions per accelerator to avoid conflicts?
