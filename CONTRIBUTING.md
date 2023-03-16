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

This process is not fully automated, the first three steps need to be performed manually by project maintainers running the `bump` command (from within this repository, rather than forks, to publish the release `tag`). Release procedure is as follows:
* Create a branch in this repository (not from fork)
* Run `cz bump [version]` (version number is optional, `cz` would automatically increment it if not present)
* Check the updated `CHANGELOG.md`
* Create and merge the branch to master
