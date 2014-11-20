memvis
======

Visualizations of application accesses

HOWTO

```
% git clone https://github.com/znik/memvis.git
% make
```

Then you have two options: to analyze trace written in a file as follows:
```
% ./analyze -f tracefile.json
```
or read the trace data from the stdin:
```
% ./your_trace_producer | ./analyzer
```
For the last case you can also supply information to be associated with the file (e.g. memo):
```
% ./you_trace_producer | ./analyzer -p "Sharing-Mystery-1.json file, that caused problems"
```

After trace processing is done, everything for visualization will be stored in a folder "server". It should be copied to the server and then the following should be executed:
```
% cd server
% ./run_server.sh
```

Now the visualization is accessible from any browser by going to "server_name:8080" ("localhost:8080" if running locally).
