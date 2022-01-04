from esp_docs.conf_docs import *  # noqa: F403,F401

extensions += ['sphinx_copybutton',
               # Needed as a trigger for running doxygen
               'esp_docs.esp_extensions.dummy_build_system',
               'esp_docs.esp_extensions.run_doxygen',
               ]

# link roles config
github_repo = 'espressif/esp-idf'

# context used by sphinx_idf_theme
html_context['github_user'] = 'espressif'
html_context['github_repo'] = 'esp-docs'

# Extra options required by sphinx_idf_theme
project_slug = 'esp-idf' # >=5.0
versions_url = 'https://dl.espressif.com/dl/esp-idf/idf_versions.js' 

idf_targets = ['esp32'] # Empty?
languages = ['en']
