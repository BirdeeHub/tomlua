# Changelog

## [1.0.1](https://github.com/BirdeeHub/tomlua/compare/v1.0.0...v1.0.1) (2025-10-14)


### Bug Fixes

* **tables:** fixed edge case that allowed duplicate tables to be valid ([9dccf39](https://github.com/BirdeeHub/tomlua/commit/9dccf39477234dce645a00642a28ffab7d1953c2))

## [1.0.0](https://github.com/BirdeeHub/tomlua/compare/v0.0.15...v1.0.0) (2025-10-04)


### Features

* **release:** v1.0.0 ([8f2385f](https://github.com/BirdeeHub/tomlua/commit/8f2385f6194255009d9b5d48628b536993a20243))

## [0.0.15](https://github.com/BirdeeHub/tomlua/compare/v0.0.14...v0.0.15) (2025-10-04)


### Bug Fixes

* **decode_defaults:** now always recursively updates the defaults if provided ([6d15b51](https://github.com/BirdeeHub/tomlua/commit/6d15b51c146b36813e432c7847a954d3bfa82f04))
* **decode_defaults:** now always recursively updates the defaults, heading or inline ([68f610f](https://github.com/BirdeeHub/tomlua/commit/68f610fa60ac4ebf0b5d79ecfcf842827d6220a8))

## [0.0.14](https://github.com/BirdeeHub/tomlua/compare/v0.0.13...v0.0.14) (2025-10-03)


### Bug Fixes

* **encode:** mixed/sparse tables ([e48cfee](https://github.com/BirdeeHub/tomlua/commit/e48cfee50eded34eb333038480a1bdf858e99c9f))
* **encode:** mixed/sparse tables ([cc1bb10](https://github.com/BirdeeHub/tomlua/commit/cc1bb10559e8f3679e0900070cd03cfb4219ee3e))

## [0.0.13](https://github.com/BirdeeHub/tomlua/compare/v0.0.12...v0.0.13) (2025-10-01)


### Bug Fixes

* **decode_to_defaults:** == to != ([a4d4c85](https://github.com/BirdeeHub/tomlua/commit/a4d4c8522ed31ffc96b4da424e153086b66fe9cf))

## [0.0.12](https://github.com/BirdeeHub/tomlua/compare/v0.0.11...v0.0.12) (2025-10-01)


### Bug Fixes

* **decode:** all decode bugs fixed, but unsure about API again ([54769b4](https://github.com/BirdeeHub/tomlua/commit/54769b4abe2a67051eb21dde93785ffddc8b1f97))
* **decode:** all decode bugs fixed, but unsure about API again ([c0ea87e](https://github.com/BirdeeHub/tomlua/commit/c0ea87e8349737343ba776df27e16a0d039cac74))

## [0.0.11](https://github.com/BirdeeHub/tomlua/compare/v0.0.10...v0.0.11) (2025-09-26)


### Bug Fixes

* **errors:** better error messages for decode ([4917d5b](https://github.com/BirdeeHub/tomlua/commit/4917d5bbb435a0fe2a6cff26b848bf98cde4a5b1))

## [0.0.10](https://github.com/BirdeeHub/tomlua/compare/v0.0.9...v0.0.10) (2025-09-26)


### Bug Fixes

* **errors:** better error messages at EOF ([f122628](https://github.com/BirdeeHub/tomlua/commit/f12262866191bbc843290988f6dc80830bfaad80))

## [0.0.9](https://github.com/BirdeeHub/tomlua/compare/v0.0.8...v0.0.9) (2025-09-25)


### Bug Fixes

* **types:** type info now accessible ([a38e9a0](https://github.com/BirdeeHub/tomlua/commit/a38e9a0d7e07bd61603ba40880cbf5327ffcf912))

## [0.0.8](https://github.com/BirdeeHub/tomlua/compare/v0.0.7...v0.0.8) (2025-09-25)


### Bug Fixes

* **inline_decode:** inline tables are closed in all situations ([d553817](https://github.com/BirdeeHub/tomlua/commit/d553817db9691d04a3bc66937b9f5775916e73dd))

## [0.0.7](https://github.com/BirdeeHub/tomlua/compare/v0.0.6...v0.0.7) (2025-09-24)


### Bug Fixes

* **encode:** should now output more headings rather than inline toml ([9cf77a7](https://github.com/BirdeeHub/tomlua/commit/9cf77a7868d196fd6676cb08c6410963af084195))

## [0.0.6](https://github.com/BirdeeHub/tomlua/compare/v0.0.5...v0.0.6) (2025-09-22)


### Bug Fixes

* **dates:** dates fixed ([5a5bfd3](https://github.com/BirdeeHub/tomlua/commit/5a5bfd3354e77d48dfa985bbfd0cc770c59f176f))

## [0.0.5](https://github.com/BirdeeHub/tomlua/compare/v0.0.4...v0.0.5) (2025-09-22)


### Bug Fixes

* **spec:** inline properly prevents further setting in all cases now ([d20dd52](https://github.com/BirdeeHub/tomlua/commit/d20dd5277df3683bfb200310b87eeb6e56a95152))

## [0.0.4](https://github.com/BirdeeHub/tomlua/compare/v0.0.3...v0.0.4) (2025-09-22)


### Bug Fixes

* **dates:** dates improved (but not fixed entirely) ([43744c5](https://github.com/BirdeeHub/tomlua/commit/43744c510245db75cc3afd227fa0bbf641fe8be3))

## [0.0.3](https://github.com/BirdeeHub/tomlua/compare/v0.0.2...v0.0.3) (2025-09-22)


### Bug Fixes

* **debug_method:** debug method doesnt build everywhere apparently ([080e933](https://github.com/BirdeeHub/tomlua/commit/080e933274d8e5805a59fde38f8e557c1bbb441e))

## [0.0.2](https://github.com/BirdeeHub/tomlua/compare/v0.0.1...v0.0.2) (2025-09-22)


### Bug Fixes

* **array_headings:** ok, biggest bug down ([9a99364](https://github.com/BirdeeHub/tomlua/commit/9a9936469610dce128bdc60990f20ae9e3dfca85))

## [0.0.1](https://github.com/BirdeeHub/tomlua/compare/v0.0.0...v0.0.1) (2025-09-21)


### Bug Fixes

* **encode:** mixed table output ([39c0edb](https://github.com/BirdeeHub/tomlua/commit/39c0edb09e0825999260c439e76783ffa133d525))

## 0.0.0 (2025-09-20)


### Bug Fixes

* **opts:** fix fail from adding overflow errors ([9f29832](https://github.com/BirdeeHub/tomlua/commit/9f29832e8d58983b017e23a4888d0d5717c2cd6c))
