# Information for Contributors

Contributions in the form of pull requests, issue reports, and feature requests are welcome!

## Updating ASIO

ASIO is managed as a submodule, to updated the version first the [espressif](github.com/espressif/asio) ASIO fork must be updated.

## Release process

When releasing a new component version we have to:

* Update the submodule reference
* Update the version number
* Update the changelog

And the automation process takes care of the last steps:

* Create the version tag in this repository
* Deploy the component to component registry
* Update the documentation

This process needs to be manually handled for ASIO component since commitizen doesn't accept the versioning schema used.

* Increment manually the version in the [manifest file](idf_component.yml)
* Export environment variables for changelog generation:
    - CZ_PRE_CURRENT_TAG_VERSION
    - CZ_PRE_NEW_TAG_VERSION
    - CZ_PRE_NEW_VERSION
* Run `python ../../ci/changelog.py asio` from this directory to generate the change log
* Check the updated `CHANGELOG.md`
* Commit the changes with the adequated message format.
    ```
    bump(asio): $current_version -> $new_version

    $Changelog for the version
    ```
* Create a PR

Once the PR is merged, the CI job tags the merge commit, creates a new release, builds and deploys documentation and the new component to the component registry
