/// \file Benchmark.cxx
/// \brief Command-line utility for benchmarking configuration backends.
///
/// A rough port of old benchmarking code to use the Configuration library
///
/// \author Pascal Boeschoten (pascal.boeschoten@cern.ch)

#include <unistd.h>
#include <limits.h>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <map>
#include <set>
#include <vector>
#include "Configuration/ConfigurationFactory.h"
#include "Monitoring/MonitoringFactory.h"

namespace {

#define PARAM_MODE_SEPARATE "separate"
#define PARAM_MODE_COMBINED "combined"
#define PARAM_MODE_FLAT "flat"
#define PARAM_MODE_TREE "tree"

using namespace AliceO2;
namespace po = boost::program_options;
using ParameterMap = std::map<std::string, std::string>;
using KeySet = std::set<std::string>;

struct Options
{
    std::vector<std::string> serverUris;
    std::string monitoringConfigUri;
    std::string runId;
    std::string parameterStructure;
    int parameterNumber;
    int processNumber;
    bool skipWait;
    bool skipCheckValues;
    bool put;
    bool printParams;
    bool help;
    bool verbose;
};

bool sVerbose = true;

auto log() -> std::ostream&
{
  static std::ofstream deadStream; // Unopened stream is essentially a '/dev/null'
  return sVerbose ? std::cout : deadStream;
}

auto getOptions(int argc, char** argv) -> Options
{
  Options options;
  std::string serverUris;
  std::string argumentsUri;

  auto optionsDescription = po::options_description("Options");
  optionsDescription.add_options()
      ("help",
          po::bool_switch(&options.help)->default_value(false),
          "Print help")
      ("verbose",
          po::bool_switch(&options.verbose)->default_value(false),
          "Verbose output")
      ("args-uri",
          po::value<std::string>(&argumentsUri),
          "URI for program arguments. Additional ones given through the command line should not conflict with them.")
      ("server-uri",
          po::value<std::string>(&serverUris),
          "Server URI. Can give multiple separated by comma. Get mode will 'randomly' pick server based on PID, "
          "put mode will put to all servers")
      ("mon-uri",
          po::value<std::string>(&options.monitoringConfigUri),
         "URI for Monitoring configuration")
      ("n-processes",
          po::value<int>(&options.processNumber)->default_value(1),
          "Number of processes")
      ("n-parameters",
          po::value<int>(&options.parameterNumber)->default_value(1),
          "Number of parameters per process")
      ("structure",
          po::value<std::string>(&options.parameterStructure)->default_value(PARAM_MODE_SEPARATE),
          "Parameter structure ['" PARAM_MODE_SEPARATE "', '" PARAM_MODE_COMBINED "', '" PARAM_MODE_FLAT "', '"
          PARAM_MODE_TREE "']")
      ("run-id",
          po::value<std::string>(&options.runId)->default_value(""),
          "Optional extra ID for result logs, e.g. for identifying a run")
      ("skip-wait",
          po::bool_switch(&options.skipWait),
          "Skip wait until simulated start")
      ("skip-check",
          po::bool_switch(&options.skipCheckValues),
          "Skip checking values returned form server")
      ("put",
          po::bool_switch(&options.put),
          "Put to server instead of get, also skips wait")
      ("print-params",
          po::bool_switch(&options.printParams),
          "Print the parameter data in csv format and exit");

  auto map = po::variables_map();
  po::store(po::parse_command_line(argc, argv, optionsDescription), map);
  po::notify(map);

  if (options.help) {
    std::cout << optionsDescription << '\n';
  }

  if (!argumentsUri.empty()) {
    // Get the arguments from the URI
    auto conf = Configuration::ConfigurationFactory::getConfiguration(argumentsUri);
    auto tree = conf->getRecursive("/");
    auto keyValues = Configuration::Tree::treeToKeyValues(tree);

    if (keyValues.empty()) {
      throw std::runtime_error("Arguments URI contained no arguments");
    }

    // Build up "command line" argument strings
    std::vector<std::string> argStrings;
    std::vector<const char*> args {"/dummy-value"}; // 1st argument is the "program" itself
    for (const auto& kv : keyValues) {
      std::ostringstream stream;
      std::string key = kv.first.substr(1); // Strip leading slash
      stream << "--" << key << '=' << Configuration::Tree::convert<std::string>(kv.second);
      argStrings.push_back(stream.str());
      args.push_back(argStrings.back().data());
    }

    // Store them
    po::store(po::parse_command_line(args.size(), args.data(), optionsDescription), map);
    po::notify(map);
  }

  if (serverUris.empty()) {
    throw std::runtime_error("Must specify server URI with '--uri' option");
  }

  // Server URIs may be comma-separated
  boost::split(options.serverUris, serverUris, boost::is_any_of(","), boost::token_compress_on);

  return options;
}

void waitUntilNextInterval()
{
  using std::chrono::system_clock;

  // Get current time
  auto now = system_clock::to_time_t(system_clock::now());
  auto currentTime = *std::localtime(&now);

  log() << "Current time " << currentTime.tm_hour << ':' << currentTime.tm_min << ':' << currentTime.tm_sec << '\n';

  // Set time for next 1 minute and 10 seconds
  auto nextTime = currentTime;

  if (nextTime.tm_sec < 10) {
    nextTime.tm_sec = 10;
  } else {
    nextTime.tm_min = nextTime.tm_min + 1;
    nextTime.tm_sec = 10;
  }

  log() << "Sleeping until " << nextTime.tm_hour << ':' << nextTime.tm_min << ':' << nextTime.tm_sec << '\n';

  // Sleep until next minute
  std::this_thread::sleep_until(system_clock::from_time_t(mktime(&nextTime)));
}

std::string flatParameterPath(int nParameters)
{
  return "/test/flat" + boost::lexical_cast<std::string>(nParameters);
}

std::string treeParameterPath(int nParameters)
{
  return "/test/tree" + boost::lexical_cast<std::string>(nParameters);
}

std::string makeValue(int number)
{
  std::stringstream stringstream;
  stringstream << "value" << std::setw(95) << std::setfill('0') << number;
  return stringstream.str();
}

int checkReturnedParameters(ParameterMap& generatedMap, ParameterMap& returnedMap)
{
  int mismatches = 0;

  if (generatedMap.size() != returnedMap.size()) {
    log() << "Mismatch of size"
        << " generated:" << generatedMap.size()
        << " returned:" << returnedMap.size() << '\n';
  }

  for (auto& parameter : generatedMap) {
    if (returnedMap.count(parameter.first) == 0) {
      // They key does not exist in the returned list
      mismatches++;
      log() << "Mismatch for key:" << parameter.first
          << " not found in returned list\n";
      continue;
    }

    if (returnedMap[parameter.first] != parameter.second) {
      // The values are not identical
      mismatches++;
      log() << "Mismatch for key:" << parameter.first
          << " expected:" << parameter.second
          << " returned:" << returnedMap[parameter.first] << '\n';
      continue;
    }
  }

  return mismatches;
}

/// Creates a list of parameters and values
///
/// The test keys and values are:
/// /test/key[0...nParams - 1] -> [0...nParams - 1]
ParameterMap createParameterMapSeparate(int nParams)
{
  ParameterMap parameterMap;
  std::string keyPrefix = "/test/separate/key";
  int pathMin = 0;
  int pathMax = nParams - 1;

  for (int i = pathMin; i <= pathMax; ++i) {
    parameterMap.emplace(keyPrefix + boost::lexical_cast<std::string>(i), makeValue(i));
  }

  return parameterMap;
}

/// Creates a ParameterMap with a single entry that combines multiple parameters
/// Uses 16 characters per parameter
ParameterMap createParameterMapCombined(int nParams)
{
  ParameterMap parameterMap;
  std::stringstream stringstream;

  for (int i = 0; i < nParams; ++i) {
    stringstream << "key" << i << "=value" << std::setw(95) << std::setfill('0') << i << '|';
  }

  parameterMap.emplace("/test/combined/key" + boost::lexical_cast<std::string>(nParams), stringstream.str());
  return parameterMap;
}

ParameterMap createParameterMapFlat(int nParameters)
{
  ParameterMap parameterMap;
  std::string pathPrefix = flatParameterPath(nParameters) + "/";

  for (int i = 0; i < nParameters; ++i) {
    parameterMap.emplace(pathPrefix + "key" + boost::lexical_cast<std::string>(i), makeValue(i));
  }

  return parameterMap;
}

/// Recursive helper function for createParameterMapTree()
void _createParameterMapTreeRecursive(
    const int nParameters,
    int& currentParameters,
    const int neededDepth,
    int currentDepth,
    const std::string currentDirKey,
    const int maxParametersPerDirectory,
    ParameterMap& parameterMap)
{
  if (currentDepth > neededDepth) {
    return;
  }

  if (currentParameters >= nParameters) {
    return;
  }

  int addedParameters = 0;
  while (currentParameters < nParameters && addedParameters < 5) {
    parameterMap.emplace(currentDirKey + "/key" + boost::lexical_cast<std::string>(currentParameters),
        makeValue(currentParameters));

    currentParameters++;
    addedParameters++;
  }

  _createParameterMapTreeRecursive(nParameters, currentParameters, neededDepth, currentDepth + 1,
      currentDirKey + "/dirA", maxParametersPerDirectory, parameterMap);

  _createParameterMapTreeRecursive(nParameters, currentParameters, neededDepth, currentDepth + 1,
      currentDirKey + "/dirB", maxParametersPerDirectory, parameterMap);
}

ParameterMap createParameterMapTree(int nParameters)
{
  ParameterMap parameterMap;
  std::string currentDirKey = treeParameterPath(nParameters);

  int currentParameters = 0;

  auto findDepth = [](int nParameters, int paramsPerLevel){
    int depth = 0;
    int maxElements = 0;
    for (;;) {
      maxElements += ::pow(2, depth) * paramsPerLevel;

      if (nParameters <= maxElements) {
        return depth;
      }

      depth++;
    }
  };

  int maxParametersPerDirectory = 5;  int neededDepth = findDepth(nParameters, maxParametersPerDirectory);
  int currentDepth = 0;

  _createParameterMapTreeRecursive(nParameters, currentParameters, neededDepth, currentDepth, currentDirKey,
      maxParametersPerDirectory, parameterMap);

  return parameterMap;
}

void putParametersToServer(Configuration::ConfigurationInterface* configuration, const ParameterMap& parameterMap)
{
  log() << "Putting key-values: \n";
  for (const auto& kv : parameterMap) {
    log() << " - " << kv.first << " -> " << kv.second << '\n';
    configuration->putString(kv.first, kv.second);
  }
}

ParameterMap getParametersFromServer(Configuration::ConfigurationInterface* configuration, const ParameterMap& keys)
{
  ParameterMap map;
  log() << "Getting keys: \n";
  for (const auto& kv : keys) {
    log() << " - " << kv.first << '\n';
    if (auto value = configuration->getString(kv.first)) {
      map.emplace(kv.first, *value);
    } else {
      throw std::runtime_error("Failed to get key '" + kv.first + "'");
    }
  }
  return map;
}

ParameterMap getParametersFromServerRecursive(Configuration::ConfigurationInterface* configuration, std::string key)
{
  ParameterMap map;
  log() << "Getting recursive: " << key << '\n';
  Configuration::Tree::Node node = configuration->getRecursive(key);
  auto keyValues = Configuration::Tree::treeToKeyValues(node);
  for (const auto& kv : keyValues) {
    map.emplace(key + kv.first, Configuration::Tree::convert<std::string>(kv.second));
  }
  return map;
}

// Abstract base class for handling different parameter structures: how to put them, get them and check the results
class ParameterHandler
{
  public:
    virtual ~ParameterHandler()
    {
    }

    virtual void put(Configuration::ConfigurationInterface* configuration, int nParameters)
    {
      auto parameterMap = createParameterMap(nParameters);
      putParametersToServer(configuration, parameterMap);
    }

    virtual void get(Configuration::ConfigurationInterface* configuration, int nParameters)
    {
      generatedMap = createParameterMap(nParameters);
      returnedMap = getParametersFromServer(configuration, generatedMap);
    }

    virtual int check()
    {
      return checkReturnedParameters(generatedMap, returnedMap);
    }

    virtual ParameterMap createParameterMap(int nParameters) = 0;

    ParameterMap generatedMap;
    ParameterMap returnedMap;
};

/// One query per parameter
class SeparateParameterHandler: public ParameterHandler
{
  private:
    virtual ParameterMap createParameterMap(int nParameters)
    {
      return createParameterMapSeparate(nParameters);
    }
};

/// One query per process, parameters combined into one string
class CombinedParameterHandler: public ParameterHandler
{
  public:
    virtual ParameterMap createParameterMap(int nParameters)
    {
      return createParameterMapCombined(nParameters);
    }
};

/// One query per process, parameters in one flat directory
class FlatParameterHandler: public ParameterHandler
{
  public:
    virtual void get(Configuration::ConfigurationInterface* configuration, int nParameters)
    {
      generatedMap = createParameterMap(nParameters);
      returnedMap = getParametersFromServerRecursive(configuration, flatParameterPath(nParameters));
    }

    virtual ParameterMap createParameterMap(int nParameters)
    {
      return createParameterMapFlat(nParameters);
    }
};

/// One query per process, parameters in tree directory structure
class TreeParameterHandler: public ParameterHandler
{
  public:
    virtual void get(Configuration::ConfigurationInterface* configuration, int nParameters)
    {
      generatedMap = createParameterMap(nParameters);
      returnedMap = getParametersFromServerRecursive(configuration, treeParameterPath(nParameters));
    }

    virtual ParameterMap createParameterMap(int nParameters)
    {
      return createParameterMapTree(nParameters);
    }
};

std::string selectUri(const Options& options)
{
    if (options.serverUris.empty()) {
      throw std::runtime_error("No server URIs specified");
    } else if (options.serverUris.size() == 1) {
      log() << "Server URI: " << options.serverUris.at(0) << '\n';
      return options.serverUris.at(0);
    } else {
      auto pid = ::getpid();
      const auto& serverUri = options.serverUris.at(pid % options.serverUris.size());
      log() << "Used PID " << pid << " to select 'round-robin' server URI: " << serverUri << '\n';
      return serverUri;
    }
}

auto getParameterHandler(const Options& options) -> std::unique_ptr<ParameterHandler>
{
  if (options.parameterStructure == PARAM_MODE_SEPARATE) {
    return std::make_unique<SeparateParameterHandler>();
  } else if (options.parameterStructure == PARAM_MODE_COMBINED) {
    return std::make_unique<CombinedParameterHandler>();
  } else if (options.parameterStructure == PARAM_MODE_FLAT) {
    return std::make_unique<FlatParameterHandler>();
  } else if (options.parameterStructure == PARAM_MODE_TREE) {
    return std::make_unique<TreeParameterHandler>();
  } else {
    throw std::runtime_error("invalid 'mode' option");
  }
}

void printMapCsv(const ParameterMap& map)
{
  for (auto& kv : map) {
    log() << kv.first << "," << kv.second << "\n";
  }
}

template <typename T>
int64_t toMillis(const T& t)
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(t).count();
}

template <typename T>
double timeToDouble(const T& t)
{
  return std::chrono::duration<double>(t).count();
}

void configureMonitoring(const Options& options)
{
//      auto conf = Configuration::ConfigurationFactory::getConfiguration(uri);
//      Configuration::Tree::printTree(conf->getRecursive("/"), std::cout);
  Monitoring::MonitoringFactory::Configure(options.monitoringConfigUri);
}

void doPut(const Options& options, ParameterHandler& parameterHandler)
{
  log() << "Putting '" << options.parameterNumber << "' parameters to servers ";
  for (const auto& uri : options.serverUris) {
    log() << "'" << uri << "' ";
  }
  log() << '\n';

  for (const auto& uri : options.serverUris) {
    auto configuration = Configuration::ConfigurationFactory::getConfiguration(uri);
    parameterHandler.put(configuration.get(), options.parameterNumber);
  }
}

void doGet(const Options& options, ParameterHandler& parameterHandler) {
  if (options.processNumber > 1) {
    log() << "Forking to get " << options.processNumber << " processes\n";
  }
  if (options.monitoringConfigUri.empty()) {
    throw std::runtime_error("Monitoring URI required");
  }

  for (int i = 1; i < options.processNumber; ++i) {
    pid_t pid = fork();
    if (pid < 0) {
      throw std::runtime_error("Fork error");
    } else if (pid == 0) {
      sVerbose = false; // Children should be silent
      break; // Children exit loop
    }
    // Parent continues
  }

  configureMonitoring(options);

  // Wait for next minute if required
  if (!options.skipWait) {
    log() << "Waiting until next interval\n";
    waitUntilNextInterval();
  }

  // Get parameters from server
  log() << "Getting from server\n";
  std::string uri = selectUri(options);
  auto configuration = Configuration::ConfigurationFactory::getConfiguration(uri);
  auto startTime = std::chrono::system_clock::now();
  parameterHandler.get(configuration.get(), options.parameterNumber);
  auto endTime = std::chrono::system_clock::now();

  try {
    auto start = toMillis(startTime.time_since_epoch());
    auto end = toMillis(endTime.time_since_epoch());

    const std::vector<Monitoring::Tag> tags {
      {"process.number", std::to_string(options.processNumber)},
      {"param.number", std::to_string(options.parameterNumber)},
      {"param.structure", options.parameterStructure},
    };

    auto& monitoring = Monitoring::MonitoringFactory::Get();
    monitoring.sendTagged<int64_t>(start, "time", std::vector<Monitoring::Tag>(tags));
    monitoring.sendTagged<int64_t>(end, "time", std::vector<Monitoring::Tag>(tags));
  }
  catch (const std::exception& e) {
    throw std::runtime_error(std::string("Failed to send monitoring data - ") + e.what());
  }

  if (!options.skipCheckValues) {
    // Verify returned values
    log() << "Checking parameters\n";
    int mismatches = parameterHandler.check();
    if (mismatches > 0) {
      std::cout << "Mismatches found: " << mismatches << '\n';
      Monitoring::MonitoringFactory::Get().sendTagged(mismatches, "mismatches", {});
    }

    if (sVerbose) {
      log() << "# Generated\n";
      printMapCsv(parameterHandler.generatedMap);
      log() << "# Returned\n";
      printMapCsv(parameterHandler.returnedMap);
    }
  }
}
} // Anonymous namespace

int main(int argc, char** argv)
{
  try {
    const Options options = getOptions(argc, argv);
    sVerbose = options.verbose;

    if (options.help) {
      return 0;
    }

    auto parameterHandler = getParameterHandler(options);

    if (options.printParams) {
      log() << "Printing parameters\n";
      printMapCsv(parameterHandler->createParameterMap(options.parameterNumber));
    }
    else if (options.put) {
      doPut(options, *parameterHandler.get());
    }
    else {
      doGet(options, *parameterHandler.get());
    }
  } catch (const std::exception& e) {
    std::cerr << "FATAL: " << e.what() << '\n';
    return 1;
  }
}

