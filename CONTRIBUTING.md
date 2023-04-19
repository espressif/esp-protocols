# Information for Contributors

Contributions in the form of pull requests, issue reports, and feature requests are welcome!

## Submitting a PR

- [ ] Fork the [esp-protocols repository on GitHub](https://github.com/espressif/esp-protocols) to start making your changes.

- [ ] Install pre-commit hooks: `pip install pre-commit && pre-commit install-hooks && pre-commit install --hook-type commit-msg --hook-type pre-push`

- [ ] Send a pull request (PR) and work with us until it gets merged and published. Contributions may need some modifications, so a few rounds of review and fixing may be necessary.

For quick merging, the contribution should be short, and concentrated on a single feature or topic. The larger the contribution is, the longer it would take to review it and merge it.

Please follow the [conventional commits](https://www.conventionalcommits.org/en/v1.0.0/) rule when writing commit messages.

## Release process

When releasing a new component version we have to:

* Update the version number
* Update the changelog
* Create the version tag in this repository
* Deploy the component to component registry
* Update the documentation

This process is not fully automated, the first step needs to be performed manually by project maintainers running the `bump` command. Release procedure is as follows:
* Run `ci/bump [component] [version]` (version number is optional, `cz` would automatically increment it if not present)
* Check the updated `CHANGELOG.md` and the generated bump commit message
* Create a PR
Once the PR is merged, the CI job tags the merge commit, creates a new release, builds and deploys documentation and the new component to the component registry
