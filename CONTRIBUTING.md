# Contributing
- Branch from `main`, small PRs, one module per PR where possible. CI (`ruff`, unit tests, e2e) must pass.
- **Changing a contract** (`packages/schemas`): bump `version` (SemVer; breaking = major), update `docs/contracts.md`,
  and get approval from the owners of every module that imports the changed model.
- New code depends on `DroneInterface`, `Repository`, `World`, `Detector` (Protocols/ABCs), never on a concrete drone/DB.
- Keep heavy imports (torch, ultralytics) lazy or inside the `[ml]` extra so CI stays fast.
- Large binaries (Unreal assets, models, datasets) go through Git LFS or object storage, never plain git.
- Every module keeps a test that runs without Unreal, a GPU or a database.
