# http-server

To test do the following:
  - clone repo
  - compile & run server
  - open localhost:8080/index.html in browser | make request through wget, telnet or another tool

Expected result:
  - File index.html is displayed in browser (or downloaded in case of wget)
  - In case of error appropriate HTTP error code is returned
  - W3C logs are written to logs/w3c.log

Additional:
  - to change server properties (port, address, root folder) edit config
