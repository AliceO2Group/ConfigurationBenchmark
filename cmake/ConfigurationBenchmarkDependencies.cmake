
########## DEPENDENCIES lookup ############

find_package(Boost 1.56.0 COMPONENTS program_options REQUIRED)
find_package(Git QUIET) # if we don't find git or FindGit.cmake is not on the system we ignore it.
find_package(Configuration REQUIRED)
find_package(Monitoring REQUIRED)

########## Bucket definitions ############

o2_define_bucket(
    NAME
    configuration_benchmark_bucket

    DEPENDENCIES
    ${Configuration_LIBRARIES}
    ${Monitoring_LIBRARIES}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}

    SYSTEMINCLUDE_DIRECTORIES
    ${Boost_INCLUDE_DIR}
    ${Configuration_INCLUDE_DIRS}
    ${Monitoring_INCLUDE_DIRS}
)

set(BUCKET_NAME_CONFIGBENCH configuration_benchmark_bucket)

