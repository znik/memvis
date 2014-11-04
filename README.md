memvis
======

Visualizations of application accesses

HOWTO

1. git clone https://github.com/znik/memvis
2. make
3. ./analyze tracefile.json datafolder

It is recommended to set the second arg to "data",
otherwise you will have to edit script.js as follows:

10   var folder\_1st = "data"; // change "data" to <data_folder> that you supplied

(processing takes ~ 30-60 min)

4. ./run_server.sh

5. Open "localhost:8080" in your browser.
