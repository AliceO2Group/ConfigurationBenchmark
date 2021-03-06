# Benchmark (work in progress)


## Introduction
[todo] intro

Results & statistics are logged using the Monitoring library. 
The given `--mon-uri` is used to configure Monitoring.
It's only been used with InfluxDB so far.
An example configuration file is provided "example-monitoring.json". 
It is recommended to modify it to taste and copy it to an etcd or Consul instance to allow the benchmark clients to
access it easily. The Configuration library's `configuration-copy` command line utility can be used for this:
~~~
configuration-copy \
  --source=json://my_local_dir/configuration-benchmark/example-monitoring.json \
  --dest=consul://my_server:8500/conf-bench/monitoring/
~~~

In addition, there is support for using a Configuration URI for program arguments.
See the example file "example-parameters.json"
As with the monitoring configuration, it is recommended to copy this a server-based Configuration backend.

[todo] time, processes, parameters, structure, etc...

Also see --help option of the binary.


# Installation
~~~
git clone https://gitlab.cern.ch/AliceO2Group/system-configuration
cd system-configuration/ansible
ansible-playbook -i inventory/configuration-benchmark-client -t configuration-benchmark-client -s site.yml --user=root
~~~


# Usage
First put the configuration parameters into the server using the --put option.
~~~
configuration-benchmark \
  --server-uri='etcd://my_server:2379/my_dir/test' \
  --n-parameters=10 \
  --structure=tree \
  --put
~~~

Then execute the benchmark using the same configuration parameters and structure
~~~
configuration-benchmark \
  --server-uri='etcd://my_server:2379/my_dir/test' \
  --mon-uri='consul://my_server:8500/my_dir/conf-bench/monitoring/' \
  --n-processes=10 \
  --n-parameters=10 \
  --structure=tree \
  --verbose \
  --skip-wait
~~~
Unless the argument `--skip-wait` is used, the benchmark will wait until 10 seconds past the minute to start.
This simulates the "start command" situation the Configuration library will be subjected to in the real world.

You can also use Ansible to execute on multiple remote machines.
This example assumes you are using the system-configuration repo for the inventory.
~~~
ansible configuration-benchmark-client -u root -i inventory -m command -a 'bash -c "env -u http_proxy LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/o2-dependencies/lib:/opt/o2-modules/lib nohup configuration-benchmark --args-uri=consul://my_server:8500/pboescho/conf-bench/bench_0/arguments/ &"'
~~~

Using the `--args-uri` option, it's also possible to use a configuration source to supply the command line arguments.
For example:
~~~
configuration-benchmark --args-uri=consul://localhost:8500/my-dir/conf-bench/bench_0/arguments/
~~~
The benchmark will take all key-values in the given directory and interpret them exactly as the command-line options.
Additional arguments may still be given on the command-line.


# Example suite usage
This suite is meant to be used with the internal benchmark setup deployed with Ansible. 

(1) First modify the `example-bench-suite.json` file to correspond to your server/client setup.
It contains a list of benchmark "setups" referred to by an index number. 
These can be iterated through by the `example-bench-script.sh`.

(2) Copy it to e.g. a Consul server
~~~
configuration-copy \
  --source=json://my_local_dir/configuration-benchmark/example-bench-suite.json \
  --dest=consul://my_server:8500/
~~~

(3) Adjust parameters in the script if needed, and run it
~~~
cd /my/ansible/dir/
bash example-bench-script.sh
~~~

The script will iterate through the setups, executing a benchmark every minute.
 

# Notes
It's preferable to use IP addresses in the URIs instead of hostnames.
Especially with larger amounts of clients, the DNS load can be significant.
Alternatively, you could use a DNS cache: http://service-dns.web.cern.ch/service-dns/faq.asp#caching0 

