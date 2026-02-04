import os
import lit.formats

config.name = 'CaGe'

config.test_format = lit.formats.ShTest(execute_external=True)

config.suffixes = ['.c', '.cpp']

config.test_source_root = os.path.join(os.path.dirname(__file__), 'codes')

if config.has_openmp:
  config.available_features.add('openmp')

# # To find the wrapper scripts primarily:
# path = os.path.pathsep.join((config.cage_scripts_dir, config.environment['PATH']))
# config.environment['PATH'] = path

config.substitutions.append(('%cage_cc', f'{config.cage_scripts_dir}/cage-clang-test'))
config.substitutions.append(('%cage_cxx', f'{config.cage_scripts_dir}/cage-clang++-test'))
config.substitutions.append(('%filecheck', config.filecheck_path))
