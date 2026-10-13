# Contributing to 3D Eclipse Simulator

Thanks for your interest. Bug reports, ideas, documentation fixes and pull requests are all welcome.

## Reporting a bug

Open a GitHub Issue and include:

- Unreal Engine version (developed with 5.7) and Cesium for Unreal version (tested with v2.22.1);
- Windows version and GPU;
- how you ran the app (packaged build from Gumroad / itch.io, or built from source);
- the eclipse, date and location you were viewing;
- the console output of the Python backend, if relevant;
- a screenshot, if the problem is visual.

Many "no data" problems are caused by the backend not running or by ports 5000–5002 being in use; see **Troubleshooting** in the README first.

## Suggesting a feature

Open an Issue describing the use case (classroom, planetarium, eclipse planning…). The roadmap in the README lists what is already planned.

## Pull requests

1. Fork the repository and create a branch from `main`.
2. Keep each pull request focused on one change.
3. For the Python backend, check that the contact times of at least one eclipse in [`validation/`](validation/) are unchanged (or explain why they changed).
4. For Unreal changes, state the engine and plugin versions you tested with. Do not commit `Binaries/`, `Intermediate/`, `Saved/` or `DerivedDataCache/`.
5. Do not add third-party assets without a licence that allows redistribution; add them to [`docs/THIRD_PARTY.md`](docs/THIRD_PARTY.md) in the same pull request.

By contributing you agree that your contribution is released under the Apache License 2.0, like the rest of the project.