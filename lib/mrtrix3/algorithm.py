# Set of functionalities for when a single script has many 'algorithms' that may be invoked,
#   i.e. the script deals with generating a particular output, but there are a number of
#   processes to select from, each of which is capable of generating that output.



# Helper function for finding where the files representing different script algorithms will be stored
# These will be in a sub-directory relative to this library file
def _algorithmsPath():
  import os
  from mrtrix3 import path
  print (__file__)
  return os.path.join(os.path.dirname(__file__), path.scriptSubDirName())



# This function needs to be safe to run in order to populate the help page; that is, no app initialisation has been run
def getList():
  import os, sys
  from mrtrix3 import message
  global _algorithm_list
  algorithm_list = [ ]
  src_file_list = os.listdir(_algorithmsPath())
  for filename in src_file_list:
    filename = filename.split('.')
    if len(filename) == 2 and filename[1] == 'py' and not filename[0] == '__init__':
      algorithm_list.append(filename[0])
  algorithm_list = sorted(algorithm_list)
  message.debug('Found algorithms: ' + str(algorithm_list))
  return algorithm_list



def initialise(base_parser, subparsers):
  import pkgutil
  from mrtrix3 import message
  initlist = [ ]
  for importer, package_name, ispkg in pkgutil.iter_modules( [ _algorithmsPath() ] ):
    loader = importer.find_loader(package_name)
    module = loader[0].load_module(package_name)
    module.initParser(subparsers, base_parser)
    initlist.extend(package_name)
  message.debug('Initialised algorithms: ' + str(initlist))




def getModule(name):
  import pkgutil
  # TODO Is this causing a double module load?
  return pkgutil.find_loader(name).load_module()

