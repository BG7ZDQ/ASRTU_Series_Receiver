# GNU Radio 3.10.1 exports a dependency on Python::Module before creating
# that imported target. Keep the OOT configure compatible with Ubuntu 22.04.
find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
