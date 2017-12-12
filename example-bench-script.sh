#!/bin/bash

# Range of setups to execute for benchmark
bench_setup_from=8
bench_setup_to=11

# Range of setups to execute for putting variables to the server
put_setup_from=0
put_setup_to=-1

inventory=inventory/hosts
put_host=myclient001
bench_hosts=configuration-benchmark-clients

# Script for the client machines
bench_script=/tmp/bench-script.sh
rm -f $bench_script
echo "#!/bin/bash" >> $bench_script
echo "export MODULEPATH=/opt/alisw/el7/modulefiles:$MODULEPATH" >> $bench_script
echo "module load Configuration-Benchmark/v0.2.0-1" >> $bench_script
echo "unset http_proxy" >> $bench_script
echo "configuration-benchmark --args-uri=consul://localhost:8500/conf-bench/setups/\$1/args/ \$2 \$3 \$4 \$5" >> $bench_script

function do_sleep
{
  # Sleep until 50 seconds past the minute
  seconds=$(($(date +%s) % 60))
  if [ "$seconds" -lt 50 ]
  then
    sleep $((50 - $seconds))
  else
    sleep $((50 + 60 - $seconds))
  fi
}

set -x

# Copy script
ansible $bench_hosts -u root -i $inventory -m copy -a "src=$bench_script dest=$bench_script"

# Do put operations (for now, the first 4 cover all the data structures)
for ((i=$put_setup_from; i<=$put_setup_to; ++i)); do
  launch_bench $i --put
  ansible $put_host -u root -i $inventory -m command -a "bash $bench_script $i --put"
done

# Benchmarks! Forever!
while [ 1 ]; do
  for ((i=$bench_setup_from; i<=$bench_setup_to; ++i)); do
    do_sleep
    # Use this for debugging
    #ansible $bench_hosts -u root -i $inventory -m command -a "bash $bench_script $i" &
    # This for no output (better for production). Can still check client's /var/spool/mail/root for messages
    ansible $bench_hosts -u root -i $inventory -m shell -a "echo \"bash -i $bench_script $i\" | at now"
  done
done
