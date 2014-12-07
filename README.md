# MEMVIS

### SUMMARY

MEMVIS parses traces in json-like format that is described [here](https://github.com/datachi/memdb#understanding-memtracker-traces), analyzes them to find true sharing and false sharing problems and produces an HTML page showing an interactive summary.

MEMVIS analyzes the trace and produces descriptor files that should be placed to server/data folder. Then, the "server" folder should be copied to a web-server so to be accessible for viewing from browsers.

In the address string there is an argument that regulates the sharing threshold. Variables that have lower sharing intensity won't be shown:
```C++
"localhost:8080/index.html?threshold=0.05"
```

### HOWTO

Take the source code:
```
% git clone https://github.com/znik/memvis.git
% make
```

Then you have two options:
1) to analyze trace written in a file as follows:
```
% ./analyze -f tracefile.json
```
or
2) read the trace data from the stdin:
```
% ./your_trace_producer | ./analyzer
```

You can also supply information to be associated with the file (e.g. memo):
```
% ./your_trace_producer | ./analyzer -p "file, that caused problems in v0.9.1"
```

When trace processing is done, everything for visualization will be stored in the folder "server". It should be copied to the server and then the following should be executed to run server locally (python is required to be installed):

Unix:
```
% cd server
% ./run_server.sh
```
Windows:
```
cd server
run_server.cmd
```

Now the visualization in available in a browser.

##### Running analyzer outside of the dir it was compiled in.
```
% cp analyzer ./your_folder
% cd your_folder
% ./your_trace_producer | ./analyzer -p "My trace file" -d path/to/original/server/folder/data
```
the path is required to end with "/data".
