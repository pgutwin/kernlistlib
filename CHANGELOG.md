# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

### Added

### Removed

### Fixed

## [n/a] 20 November 2025

### Changed
- Extensive update to add
  - Builder function: API access for modifying existing parsed Verilog or building "from scratch"
  - Validation function: API checks for completeness of internal representation of netlist
  - Writer function: API to write out Verilog

## [n/a] 8 October 2025

### Fixed
- Net name slicing was broken - at least now it's fixed in the testing but need to continue to investigate

## [n/a] 22 September 2025

### Fixed
- Extended testing showed that the port naming in instances was buggered up.
  - Took quite a while to sort out the actions problem.

## [n/a] 22 September 2025

### Added
- Initial version




