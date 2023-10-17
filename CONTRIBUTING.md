# Information for Contributors

Contributions in the form of pull requests, issue reports, and feature requests are welcome!

## Common Terminology:
* [Type]: Examples include feat (for new features), fix (for bug fixes), ci (for continuous integration), bump (for version updates), etc. You can find a comprehensive list of types in .pre-commit-config.yaml on line 65.
* [Scope]: Refers to specific sections or areas within the project, such as mdns, modem, common, console, etc. You can discover additional scopes in .pre-commit-config.yaml on line 65.
* [Component]: This is the name of the component, and it should match the directory name where the component code is located.

## Submitting a PR

- [ ] Fork the [esp-protocols repository on GitHub](https://github.com/espressif/esp-protocols) to start making your changes.

- [ ] Install pre-commit hooks: `pip install pre-commit && pre-commit install-hooks && pre-commit install --hook-type commit-msg --hook-type pre-push`

- [ ] Send a pull request (PR) and work with us until it gets merged and published. Contributions may need some modifications, so a few rounds of review and fixing may be necessary.

For quick merging, the contribution should be short, and concentrated on a single feature or topic. The larger the contribution is, the longer it would take to review it and merge it.

Please follow the [conventional commits](https://www.conventionalcommits.org/en/v1.0.0/) rule when writing commit messages.

A typical commit message title template:

Template:
`[type]([scope]): Message`

e.g.
`feat(console): Added fully operational ifconfig command`


## Creating a new component

Steps:
1. Add a file named .cz.yaml to the root of the component.

The template for .cz.yaml should look like this:
```
---
commitizen:
  bump_message: 'bump([scope]): $current_version -> $new_version'
  pre_bump_hooks: python ../../ci/changelog.py [component]
  tag_format: [component]-v$version
  version: 0.0.0
  version_files:
  - idf_component.yml
```
2. Run the following command to bump the version of the component:

`ci/bump [component] [version] --bump-message "bump([scope]): First version [version]"`

Replace [component], [version] and [scope] with the specific component name, version and scope you are working with. This command will help you bump the version of the component with the provided details.

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
